#include "addons/uart_input_addon.h"
#include "storagemanager.h"
#include "hardware/gpio.h"
#include <cstring>

#ifndef LED_PIN_DEBUG
#define LED_PIN_DEBUG 25 // По умолчанию встроенный LED, проверьте для своей платы (часто 0 на кастомных, 25 на Pico)
#endif

// Простая мигалка без блокировки основного цикла надолго
void debug_blink(int count, int speed_ms) {
    for (int i = 0; i < count; i++) {
        gpio_put(LED_PIN_DEBUG, 1);
        sleep_ms(speed_ms);
        gpio_put(LED_PIN_DEBUG, 0);
        sleep_ms(speed_ms);
    }
}

bool UARTInputAddon::available() {
    const AddonOptions& addonOptions = Storage::getInstance().getAddonOptions();
    const PeripheralOptions& periphOptions = Storage::getInstance().getPeripheralOptions();
    // Проверяем включение аддона И включение соответствующего блока UART
    return addonOptions.dualPicoHostOptions.enabled && 
           ((DUAL_UART_BLOCK == 0) ? periphOptions.blockUART0.enabled : periphOptions.blockUART1.enabled);
}

void UARTInputAddon::setup() {
    isEnabled = false;

    gpio_init(LED_PIN_DEBUG);
    gpio_set_dir(LED_PIN_DEBUG, GPIO_OUT);
    gpio_put(LED_PIN_DEBUG, 0);
    
    // Получаем указатель на UART. Он всегда вернется (не null), так как PeripheralManager - синглтон.
    uart = PeripheralManager::getInstance().getUART(DUAL_UART_BLOCK);
    
    // Проверяем, настроен ли UART в PeripheralManager (вызывался ли setConfig)
    if (uart) {
        if (uart->configured) {
            isEnabled = true;
        } else {
            debug_blink(3, 100);
            return;
        }
    } else {
        debug_blink(5, 100);
        return;
    }
    

    // Длинный блинк - успешная инициализация
    debug_blink(2, 200);

    resetState();
}

void UARTInputAddon::reinit() {
    setup();
}

void UARTInputAddon::process() {
    // Основная логика в preprocess
}

void UARTInputAddon::preprocess() {
    // КРИТИЧЕСКОЕ ИСПРАВЛЕНИЕ:
    // 1. Проверяем флаг isEnabled (установленный в setup)
    // 2. Проверяем указатель на null (на всякий случай)
    // 3. НЕ проверяем isReadable(), если указатель невалиден
    
    if (!isEnabled || uart == nullptr) {
        return;
    }
    
    // Теперь безопасно вызывать методы uart
    if (!uart->isReadable()) {
        return;
    }

    readUart();
}

void UARTInputAddon::resetState() {
    rxIndex = 0;
    escaped = false;
}

void UARTInputAddon::readUart() {
    while (uart->isReadable()) {
        uint8_t c = uart->read();

        if (escaped) {
            if (rxIndex < BUFFER_SIZE) {
                if (c == ESC_END) rxBuffer[rxIndex++] = END;
                else if (c == ESC_ESC) rxBuffer[rxIndex++] = ESC;
                else rxBuffer[rxIndex++] = c;
            }
            escaped = false;
        } else {
            if (c == END) {
                if (rxIndex > 4) { 
                    uint32_t receivedCRC = 0;
                    receivedCRC |= rxBuffer[rxIndex - 4];
                    receivedCRC |= rxBuffer[rxIndex - 3] << 8;
                    receivedCRC |= rxBuffer[rxIndex - 2] << 16;
                    receivedCRC |= rxBuffer[rxIndex - 1] << 24;

                    uint32_t calcCRC = calculateCRC32(rxBuffer, rxIndex - 4);

                    if (calcCRC == receivedCRC) {
                        handlePacket(rxBuffer, rxIndex - 4);
                    }
                }
                rxIndex = 0; 
            } else if (c == ESC) {
                escaped = true;
            } else {
                if (rxIndex < BUFFER_SIZE) {
                    rxBuffer[rxIndex++] = c;
                } else {
                    rxIndex = 0;
                    escaped = false;
                }
            }
        }
    }
}

void UARTInputAddon::handlePacket(const uint8_t* data, uint16_t len) {
    if (len == 0) return;

    DualCommand cmd = (DualCommand)data[0];

    switch (cmd) {
        case DualCommand::REQUEST_B_INIT:
            // Короткие вспышки - запрос инициализации
            debug_blink(1, 50); 
            handleRequestBInit();
            break;
        case DualCommand::REPORT_RECEIVED:
            handleReportReceived(data, len);
            break;
        case DualCommand::DEVICE_CONNECTED:
            handleDeviceConnected(data, len);
            break;
        case DualCommand::DEVICE_DISCONNECTED:
            handleDeviceDisconnected(data, len);
            break;
        default:
            break;
    }
}

void UARTInputAddon::handleRequestBInit() {
    b_init_t msg;
    msg.command = DualCommand::B_INIT;
    msg.interval_override = 0;
    sendPacket((uint8_t*)&msg, sizeof(msg));
}

void UARTInputAddon::handleDeviceConnected(const uint8_t* data, uint16_t len) {
    gpio_put(LED_PIN_DEBUG, 1); // Включаем LED постоянно при подключении
}

void UARTInputAddon::handleDeviceDisconnected(const uint8_t* data, uint16_t len) {
    gpio_put(LED_PIN_DEBUG, 0); // Выключаем LED при отключении
}

void UARTInputAddon::handleReportReceived(const uint8_t* data, uint16_t len) {
    // Инвертируем LED при каждом пакете данных для визуализации потока
    gpio_put(LED_PIN_DEBUG, !gpio_get(LED_PIN_DEBUG));
}

void UARTInputAddon::sendPacket(const uint8_t* data, uint16_t len) {
    if (!uart || !uart->isWritable()) return;

    uint32_t crc = calculateCRC32(data, len);

    uart->write(END);

    auto sendEscaped = [&](uint8_t b) {
        if (b == END) {
            uart->write(ESC);
            uart->write(ESC_END);
        } else if (b == ESC) {
            uart->write(ESC);
            uart->write(ESC_ESC);
        } else {
            uart->write(b);
        }
    };

    for (uint16_t i = 0; i < len; i++) {
        sendEscaped(data[i]);
    }

    for (int i = 0; i < 4; i++) {
        sendEscaped((crc >> (i * 8)) & 0xFF);
    }

    uart->write(END);
}

uint32_t UARTInputAddon::calculateCRC32(const uint8_t* buf, int len) {
    uint32_t c = 0xFFFFFFFF;
    for (int i = 0; i < len; i++) {
        c = crc_table[(c ^ buf[i]) & 0xFF] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFF;
}
