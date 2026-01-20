#include "addons/dual_pico_host.h"
#include "storagemanager.h"
#include "pico/stdlib.h"
#include <cstdio>

#ifndef LED_DEBUG_PIN
#define LED_DEBUG_PIN 25 
#endif

// Центр стика (для 16-бит это ~32767)
#define GAMEPAD_JOYSTICK_MID_VAL 0x7FFF 

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

    // Очистка карты устройств
    for(int i=0; i<32; i++) dev_type_map[i] = 0;
    
    // Сброс состояния ввода
    reset_host_state();

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

// --- ВНЕДРЕНИЕ ВВОДА В СИСТЕМУ ---
void DualPicoHostAddon::preprocess() {
    Gamepad *gamepad = Storage::getInstance().GetGamepad();
    
    // Накладываем наше состояние на состояние геймпада
    gamepad->state.dpad     |= _host_state.dpad;
    gamepad->state.buttons  |= _host_state.buttons;
    
    // Для стиков простая логика: если наш стик отклонен, используем его значение.
    // Если наши стики в центре (дефолт), не трогаем, чтобы работали стики самого геймпада.
    if (_host_state.lx != GAMEPAD_JOYSTICK_MID_VAL) gamepad->state.lx = _host_state.lx;
    if (_host_state.ly != GAMEPAD_JOYSTICK_MID_VAL) gamepad->state.ly = _host_state.ly;
    if (_host_state.rx != GAMEPAD_JOYSTICK_MID_VAL) gamepad->state.rx = _host_state.rx;
    if (_host_state.ry != GAMEPAD_JOYSTICK_MID_VAL) gamepad->state.ry = _host_state.ry;
}

// --- ЛОГИКА ОБРАБОТКИ ВВОДА (НОВОЕ) ---

void DualPicoHostAddon::reset_host_state() {
    _host_state.dpad = 0;
    _host_state.buttons = 0;
    _host_state.lx = GAMEPAD_JOYSTICK_MID_VAL;
    _host_state.ly = GAMEPAD_JOYSTICK_MID_VAL;
    _host_state.rx = GAMEPAD_JOYSTICK_MID_VAL;
    _host_state.ry = GAMEPAD_JOYSTICK_MID_VAL;
}

// Разбор отчета клавиатуры (Boot Protocol)
// report_data[0] = Modifiers
// report_data[1] = Reserved
// report_data[2..7] = Keycodes
void DualPicoHostAddon::process_kbd_report(const uint8_t* report_data) {
    reset_host_state();

    // 1. Обработка Модификаторов (Shift, Ctrl, Alt, GUI)
    uint8_t modifiers = report_data[0];
    
    // Пример: Left Shift как Select
    // 0x02 = Left Shift, 0x20 = Right Shift
    if (modifiers & 0x02) _host_state.buttons |= GAMEPAD_MASK_S1; 

    // 2. Обработка клавиш (6-KRO)
    for (int i = 2; i < 8; i++) {
        uint8_t code = report_data[i];
        if (code == 0) continue;

        switch (code) {
            // --- WASD ---
            case 0x1A: /* W */ _host_state.dpad |= GAMEPAD_MASK_UP;    break;
            case 0x16: /* S */ _host_state.dpad |= GAMEPAD_MASK_DOWN;  break;
            case 0x04: /* A */ _host_state.dpad |= GAMEPAD_MASK_LEFT;  break;
            case 0x07: /* D */ _host_state.dpad |= GAMEPAD_MASK_RIGHT; break;

            // --- Стрелки ---
            case 0x52: /* Up */    _host_state.dpad |= GAMEPAD_MASK_UP;    break;
            case 0x51: /* Down */  _host_state.dpad |= GAMEPAD_MASK_DOWN;  break;
            case 0x50: /* Left */  _host_state.dpad |= GAMEPAD_MASK_LEFT;  break;
            case 0x4F: /* Right */ _host_state.dpad |= GAMEPAD_MASK_RIGHT; break;

            // --- Основные кнопки (Layout под аркаду) ---
            case 0x0D: /* J */ _host_state.buttons |= GAMEPAD_MASK_B1; break; // Cross / A
            case 0x0E: /* K */ _host_state.buttons |= GAMEPAD_MASK_B2; break; // Circle / B
            case 0x18: /* U */ _host_state.buttons |= GAMEPAD_MASK_B3; break; // Square / X
            case 0x0C: /* I */ _host_state.buttons |= GAMEPAD_MASK_B4; break; // Triangle / Y

            // --- Триггеры / Бамперы ---
            case 0x0B: /* H */ _host_state.buttons |= GAMEPAD_MASK_L1; break;
            case 0x0F: /* L */ _host_state.buttons |= GAMEPAD_MASK_R1; break;
            case 0x33: /* ; */ _host_state.buttons |= GAMEPAD_MASK_R2; break;
            case 0x0A: /* G */ _host_state.buttons |= GAMEPAD_MASK_L2; break;

            // --- Меню ---
            case 0x28: /* Enter */ _host_state.buttons |= GAMEPAD_MASK_S2; break; // Start
            case 0x29: /* Esc */   _host_state.buttons |= GAMEPAD_MASK_A1; break; // Home
            case 0x2B: /* Tab */   _host_state.buttons |= GAMEPAD_MASK_S1; break; // Select

            // --- Дополнительно ---
            case 0x2C: /* Space */ _host_state.buttons |= GAMEPAD_MASK_B1; break; // Прыжок на пробел

            default: break;
        }
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
            // SUCCESS: Связь установлена.
            connection_established = true; 
            gpio_put(LED_DEBUG_PIN, 1); 
            send_b_init();
            break;

        case DualCommand::DEVICE_CONNECTED:
            if (len >= sizeof(device_connected_t)) {
                const device_connected_t* pkt = (const device_connected_t*)data;
                if (pkt->dev_addr < 32) {
                    // Сохраняем тип устройства
                    dev_type_map[pkt->dev_addr] = pkt->itf_num;

                    // 5 быстрых вспышек (CONNECT)
                    gpio_put(LED_DEBUG_PIN, 0); 
                    debug_blink(5, 50);         
                    if (connection_established) gpio_put(LED_DEBUG_PIN, 1); 
                }
            }
            break;

        case DualCommand::DEVICE_DISCONNECTED:
            if (len >= sizeof(device_disconnected_t)) {
                const device_disconnected_t* pkt = (const device_disconnected_t*)data;
                if (pkt->dev_addr < 32) {
                    dev_type_map[pkt->dev_addr] = 0;

                    // 3 средних вспышки (DISCONNECT)
                    gpio_put(LED_DEBUG_PIN, 0); 
                    debug_blink(3, 150);         
                    if (connection_established) gpio_put(LED_DEBUG_PIN, 1);
                }
            }
            break;

        case DualCommand::REPORT_RECEIVED:
            if (connection_established) {
                 const report_received_t* pkt = (const report_received_t*)data;
                 uint8_t addr = pkt->dev_addr;
                 
                 // Определяем, клавиатура ли это
                 bool is_keyboard = false;
                 
                 // 1. Проверка по карте устройств (если успели поймать CONNECT)
                 if (addr < 32 && dev_type_map[addr] == 1) { // 1 = Keyboard Interface Protocol
                     is_keyboard = true;
                 }
                 // 2. Fallback: если тип неизвестен, но длина похожа на Boot Keyboard Report
                 // Заголовок (3 байта) + Отчет (8 байт) = 11 байт
                 else if (addr < 32 && dev_type_map[addr] == 0 && len == 11) {
                     is_keyboard = true;
                 }

                 if (is_keyboard) {
                     // Короткая вспышка на каждое нажатие
                     gpio_put(LED_DEBUG_PIN, 0); busy_wait_us(200); gpio_put(LED_DEBUG_PIN, 1);
                     
                     // Обработка маппинга
                     // pkt->report - это массив байтов отчета
                     process_kbd_report(pkt->report);
                 }
            }
            break;
        default:
            break;
    }
}