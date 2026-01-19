#include "addons/dual_pico_host.h"
#include "storagemanager.h"
#include "pico/stdlib.h"
#include <cstdio>

// -- Вспомогательная функция CRC32 --
// Полином 0xEDB88320 (Ethernet, ZIP, etc.)
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
    // Аддон доступен только если включен в опциях И если UART настроен в PeripheralManager
    // Для простоты жестко проверяем UART1, как договаривались
    return options.enabled && PeripheralManager::getInstance().isUARTEnabled(1);
}

void DualPicoHostAddon::setup() {
    // 1. Сначала проверяем, включен ли UART1 в настройках Peripheral Manager
    if (!PeripheralManager::getInstance().isUARTEnabled(1)) {
        printf("[DualPicoHost] Error: UART1 is not enabled in Peripheral Mapping!\n");
        active_uart = nullptr;
        return;
    }

    // 2. Получаем объект
    PeripheralUART* pUart = PeripheralManager::getInstance().getUART(1);
    if (!pUart) {
        active_uart = nullptr;
        return;
    }

    // 3. (Опционально, но желательно) Проверяем флаг configured внутри PeripheralUART
    // Для этого нужно добавить метод isConfigured() в PeripheralUART или сделать поле public
    // В вашем коде поле configured публичное.
    if (!pUart->configured) {
        printf("[DualPicoHost] Error: UART1 is enabled but not configured properly!\n");
        active_uart = nullptr;
        return;
    }

    active_uart = pUart->getDriver();

    // Сброс буфера чтения
    rx_idx = 0;
    rx_escaped = false;

    // Отправляем B_INIT
    b_init_t msg;
    msg.command = DualCommand::B_INIT;
    msg.interval_override = 0; 
    
    // Тут безопасно, так как мы проверили, что UART инициализирован
    serial_write((uint8_t*)&msg, sizeof(msg));
    
    printf("[DualPicoHost] Setup complete. B_INIT sent.\n");
}

void DualPicoHostAddon::reinit() {
    setup();
}

void DualPicoHostAddon::process() {
    if (!active_uart) return;
    process_serial();
}

// Отправка данных с экранированием и CRC
void DualPicoHostAddon::serial_write(const uint8_t* data, uint16_t len) {
    if (!active_uart) return;

    uint32_t crc = crc32_pkt(data, len);

    // Начало пакета
    uart_putc_raw(active_uart, (char)SERIAL_END);

    // Данные
    for (int i = 0; i < len; i++) {
        serial_putc_escaped(data[i], active_uart);
    }

    // CRC (4 байта, Little Endian)
    for (int i = 0; i < 4; i++) {
        uint8_t b = (crc >> (i * 8)) & 0xFF;
        serial_putc_escaped(b, active_uart);
    }

    // Конец пакета
    uart_putc_raw(active_uart, (char)SERIAL_END);
}

void DualPicoHostAddon::serial_putc_escaped(uint8_t b, uart_inst_t* uart) {
    switch (b) {
        case SERIAL_END:
            uart_putc_raw(uart, (char)SERIAL_ESC);
            uart_putc_raw(uart, (char)SERIAL_ESC_END);
            break;
        case SERIAL_ESC:
            uart_putc_raw(uart, (char)SERIAL_ESC);
            uart_putc_raw(uart, (char)SERIAL_ESC_ESC);
            break;
        default:
            uart_putc_raw(uart, (char)b);
            break;
    }
}

void DualPicoHostAddon::process_serial() {
    // Читаем, пока есть данные в буфере UART
    while (uart_is_readable(active_uart)) {
        uint8_t c = uart_getc(active_uart);

        if (rx_escaped) {
            // Если предыдущий байт был ESC
            if (rx_idx < sizeof(rx_buffer)) {
                if (c == SERIAL_ESC_END) {
                    rx_buffer[rx_idx++] = SERIAL_END;
                } else if (c == SERIAL_ESC_ESC) {
                    rx_buffer[rx_idx++] = SERIAL_ESC;
                } else {
                    // Ошибка протокола, записываем как есть
                    rx_buffer[rx_idx++] = c;
                }
            }
            rx_escaped = false;
        } else {
            if (c == SERIAL_END) {
                // Конец пакета. Проверяем целостность.
                // Пакет должен быть минимум 1 байт (команда) + 4 байта (CRC) = 5 байт
                if (rx_idx >= 5) {
                    // CRC лежит в последних 4 байтах буфера (Little Endian)
                    uint32_t received_crc = 
                        rx_buffer[rx_idx-4] | 
                        (rx_buffer[rx_idx-3] << 8) | 
                        (rx_buffer[rx_idx-2] << 16) | 
                        (rx_buffer[rx_idx-1] << 24);

                    // Считаем CRC от начала буфера до (длина - 4)
                    uint32_t calculated_crc = crc32_pkt(rx_buffer, rx_idx - 4);

                    if (received_crc == calculated_crc) {
                        // CRC совпал, обрабатываем пакет (без последних 4 байт CRC)
                        handle_packet(rx_buffer, rx_idx - 4);
                    } else {
                        printf("[DualPicoHost] CRC Mismatch! Rx: %08X, Calc: %08X\n", received_crc, calculated_crc);
                    }
                }
                // Сбрасываем буфер для следующего пакета
                rx_idx = 0;
            } else if (c == SERIAL_ESC) {
                // Начало escape-последовательности
                rx_escaped = true;
            } else {
                // Обычный байт данных
                if (rx_idx < sizeof(rx_buffer)) {
                    rx_buffer[rx_idx++] = c;
                } else {
                    // Переполнение буфера, сбрасываем
                    rx_idx = 0;
                    printf("[DualPicoHost] Buffer overflow\n");
                }
            }
        }
    }
}

void DualPicoHostAddon::handle_packet(const uint8_t* data, uint16_t len) {
    if (len == 0) return;

    DualCommand cmd = (DualCommand)data[0];

    // Простая отладка: выводим тип команды
    switch (cmd) {
        case DualCommand::DEVICE_CONNECTED: {
            device_connected_t* msg = (device_connected_t*)data;
            printf("[DualPicoHost] Device Connected! VID: %04X PID: %04X\n", msg->vid, msg->pid);
            break;
        }
        case DualCommand::DEVICE_DISCONNECTED:
            printf("[DualPicoHost] Device Disconnected\n");
            break;
        case DualCommand::REPORT_RECEIVED:
            // Это самая частая команда (движение мыши, нажатие клавиш)
            // Пока просто логируем факт получения, чтобы не спамить в консоль слишком сильно
            // printf("[DualPicoHost] Report Received (Len: %d)\n", len);
            break;
        case DualCommand::REQUEST_B_INIT:
            printf("[DualPicoHost] Side B requested init. Resending B_INIT...\n");
            // Повторная отправка B_INIT
            b_init_t msg;
            msg.command = DualCommand::B_INIT;
            msg.interval_override = 0;
            serial_write((uint8_t*)&msg, sizeof(msg));
            break;
        default:
            printf("[DualPicoHost] Unknown command: %d\n", (int)cmd);
            break;
    }
}