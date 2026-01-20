#include "addons/dual_pico_host.h"
#include "storagemanager.h"
#include "pico/stdlib.h"
#include <cstdio>

#ifndef LED_DEBUG_PIN
#define LED_DEBUG_PIN 25 
#endif

// Вспомогательная функция для мигания
void debug_blink(int count, int speed_ms) {
    for (int i = 0; i < count; i++) {
        gpio_put(LED_DEBUG_PIN, 1);
        sleep_ms(speed_ms);
        gpio_put(LED_DEBUG_PIN, 0);
        sleep_ms(speed_ms);
    }
    sleep_ms(500); // Пауза после серии
}

static uint32_t crc32_pkt(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

bool DualPicoHostAddon::available() {
    const DualPicoHostOptions& options = Storage::getInstance().getAddonOptions().dualPicoHostOptions;
    return options.enabled;
}

void DualPicoHostAddon::setup() {
    // --- LED START ---
    gpio_init(LED_DEBUG_PIN);
    gpio_set_dir(LED_DEBUG_PIN, GPIO_OUT);
    gpio_put(LED_DEBUG_PIN, 1); sleep_ms(1000); gpio_put(LED_DEBUG_PIN, 0); sleep_ms(500);

    // --- 1. ПРИНУДИТЕЛЬНАЯ ИНИЦИАЛИЗАЦИЯ ---
    PeripheralManager::getInstance().initUART();

    // --- 2. ПРОВЕРКА НАСТРОЕК ---
    const PeripheralOptions& periphOptions = Storage::getInstance().getPeripheralOptions();
    
    if (!periphOptions.blockUART1.enabled) {
        debug_blink(2, 300);
        active_uart = nullptr;
        return;
    }

    if (periphOptions.blockUART1.txPin == -1 || periphOptions.blockUART1.rxPin == -1) {
        debug_blink(3, 300);
        active_uart = nullptr;
        return;
    }

    // --- 3. ПОЛУЧЕНИЕ ОБЪЕКТА ---
    PeripheralUART* pUart = PeripheralManager::getInstance().getUART(1);
    if (!pUart) {
        debug_blink(10, 100); 
        active_uart = nullptr;
        return;
    }

    // --- 4. ПРОВЕРКА СТАТУСА ---
    if (!pUart->configured) {
        debug_blink(4, 300);
        active_uart = nullptr;
        return;
    }

    // --- 5. УСПЕХ ---
    debug_blink(5, 100);

    active_uart = pUart->getDriver();
    rx_idx = 0;
    rx_escaped = false;
    connection_established = false;
    last_handshake_sent = 0;
    test_button_pressed = false;

    // Очистка карты устройств
    for(int i=0; i<32; i++) dev_type_map[i] = 0;

    send_b_init();
}

void DualPicoHostAddon::reinit() {
    setup();
}

void DualPicoHostAddon::process() {
    if (!active_uart) return;

    uint32_t now = to_ms_since_boot(get_absolute_time());

    // HEARTBEAT
    if (!connection_established && (now - last_handshake_sent > 1000)) {
        gpio_put(LED_DEBUG_PIN, 1);
        busy_wait_us(10000); 
        gpio_put(LED_DEBUG_PIN, 0);
        
        send_b_init();
        last_handshake_sent = now;
    }

    process_serial();
}

void DualPicoHostAddon::preprocess() {
    if (test_button_pressed) {
        Storage::getInstance().GetGamepad()->state.buttons |= GAMEPAD_MASK_B1;
    }
}

// --- UART TX ---

void DualPicoHostAddon::send_b_init() {
    b_init_t msg;
    msg.command = DualCommand::B_INIT;
    msg.interval_override = 0; 
    serial_write((uint8_t*)&msg, sizeof(msg));
}

void DualPicoHostAddon::serial_putc_escaped(uint8_t b, uart_inst_t* uart) {
    switch (b) {
        case SERIAL_END: uart_putc_raw(uart, (char)SERIAL_ESC); uart_putc_raw(uart, (char)SERIAL_ESC_END); break;
        case SERIAL_ESC: uart_putc_raw(uart, (char)SERIAL_ESC); uart_putc_raw(uart, (char)SERIAL_ESC_ESC); break;
        default: uart_putc_raw(uart, (char)b); break;
    }
}

void DualPicoHostAddon::serial_write(const uint8_t* data, uint16_t len) {
    if (!active_uart) return;
    uint32_t crc = crc32_pkt(data, len);
    uart_putc_raw(active_uart, (char)SERIAL_END);
    for (int i = 0; i < len; i++) serial_putc_escaped(data[i], active_uart);
    for (int i = 0; i < 4; i++) serial_putc_escaped((crc >> (i * 8)) & 0xFF, active_uart);
    uart_putc_raw(active_uart, (char)SERIAL_END);
}

// --- UART RX ---

void DualPicoHostAddon::process_serial() {
    int bytes_read = 0;
    while (uart_is_readable(active_uart) && bytes_read < 64) {
        uint8_t c = uart_getc(active_uart);
        bytes_read++;

        // RX ACTIVITY
        if (!connection_established) {
            gpio_put(LED_DEBUG_PIN, !gpio_get(LED_DEBUG_PIN));
        }

        if (rx_escaped) {
            rx_escaped = false;
            if (c == SERIAL_ESC_END) c = SERIAL_END;
            else if (c == SERIAL_ESC_ESC) c = SERIAL_ESC;
            if (rx_idx < sizeof(rx_buffer)) rx_buffer[rx_idx++] = c;
        } else {
            if (c == SERIAL_END) {
                if (rx_idx > 4) { 
                    uint32_t rcv_crc = rx_buffer[rx_idx-4] | (rx_buffer[rx_idx-3] << 8) | (rx_buffer[rx_idx-2] << 16) | (rx_buffer[rx_idx-1] << 24);
                    uint32_t calc_crc = crc32_pkt(rx_buffer, rx_idx - 4);
                    if (rcv_crc == calc_crc) {
                        handle_packet(rx_buffer, rx_idx - 4);
                    } 
                }
                rx_idx = 0;
            } else if (c == SERIAL_ESC) {
                rx_escaped = true;
            } else {
                if (rx_idx < sizeof(rx_buffer)) rx_buffer[rx_idx++] = c;
                else rx_idx = 0;
            }
        }
    }
}

void DualPicoHostAddon::handle_packet(const uint8_t* data, uint16_t len) {
    if (len == 0) return;
    DualCommand cmd = (DualCommand)data[0];

    switch (cmd) {
        case DualCommand::REQUEST_B_INIT:
            // SUCCESS: Связь установлена. Горим постоянно.
            connection_established = true; 
            gpio_put(LED_DEBUG_PIN, 1); 
            send_b_init();
            break;

        // --- НОВАЯ ЛОГИКА ПОДКЛЮЧЕНИЯ УСТРОЙСТВ ---
        case DualCommand::DEVICE_CONNECTED:
            if (len >= sizeof(device_connected_t)) {
                const device_connected_t* pkt = (const device_connected_t*)data;
                if (pkt->dev_addr < 32) {
                    // Сохраняем тип устройства
                    dev_type_map[pkt->dev_addr] = pkt->itf_num;

                    // СПЕЦИАЛЬНЫЙ СИГНАЛ: 5 быстрых вспышек при подключении клавы/мыши
                    // Это подтвердит, что ивент обработан
                    gpio_put(LED_DEBUG_PIN, 0); // выключаем
                    debug_blink(6, 100);
                    if (connection_established) gpio_put(LED_DEBUG_PIN, 1); // возвращаем свет
                }
            }
            break;

        case DualCommand::DEVICE_DISCONNECTED:
            if (len >= sizeof(device_disconnected_t)) {
                const device_disconnected_t* pkt = (const device_disconnected_t*)data;
                if (pkt->dev_addr < 32) {
                    // Очищаем запись о устройстве
                    dev_type_map[pkt->dev_addr] = 0;

                    // 3 средних вспышки (DISCONNECT)
                    gpio_put(LED_DEBUG_PIN, 0); 
                    debug_blink(3, 150);         
                    if (connection_established) gpio_put(LED_DEBUG_PIN, 1);
                }
            }
            break;
        // -------------------------------------------

        case DualCommand::REPORT_RECEIVED:
            if (connection_established) {
                 gpio_put(LED_DEBUG_PIN, 0);
                 busy_wait_us(200); 
                 gpio_put(LED_DEBUG_PIN, 1);
                 
                 // Test Spacebar (0x2C) - старая логика
                 if (len >= 3 + 8) {
                     bool space = false;
                     for (int i = 5; i < 11; i++) { 
                         if (data[i] == 0x2C) { 
                             space = true; break; 
                         }
                     }
                     test_button_pressed = space;
                 }
            }
            break;
        default:
            break;
    }
}