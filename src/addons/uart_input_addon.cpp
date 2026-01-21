#include "addons/uart_input_addon.h"
#include "storagemanager.h"
#include "hardware/gpio.h"
#include <cstring>

#ifndef LED_PIN_DEBUG
#define LED_PIN_DEBUG 25
#endif

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
    return addonOptions.dualPicoHostOptions.enabled && 
           ((DUAL_UART_BLOCK == 0) ? periphOptions.blockUART0.enabled : periphOptions.blockUART1.enabled);
}

void UARTInputAddon::setup() {
    isEnabled = false;

    gpio_init(LED_PIN_DEBUG);
    gpio_set_dir(LED_PIN_DEBUG, GPIO_OUT);
    gpio_put(LED_PIN_DEBUG, 0);
    
    uart = PeripheralManager::getInstance().getUART(DUAL_UART_BLOCK);
    
    if (!uart || !uart->configured) {
        debug_blink(5, 100);
        return;
    }

    handler.setSendCallback([this](const uint8_t* data, uint16_t len){
        this->sendPacket(data, len);
    });
    
    slip.reset();
    debug_blink(2, 200);
    isEnabled = true;
}

void UARTInputAddon::reinit() {
    setup();
}

void UARTInputAddon::preprocess() {
    
    if (!isEnabled || !uart) return;

    while (uart->isReadable()) {
        auto res = slip.push(uart->read());
        if (res == SlipFrameDecoder::Result::FRAME_OK) {
            handler.handlePacket(slip.frameData(), slip.frameSize());
        }
    }
}

void UARTInputAddon::sendPacket(const uint8_t* data, uint16_t len) {
    if (!uart || !uart->isWritable()) return;

    uint32_t crc = slip.calculateCRC32(data, len);

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

    for (uint16_t i = 0; i < len; i++) sendEscaped(data[i]);
    for (int i = 0; i < 4; i++) sendEscaped((crc >> (i * 8)) & 0xFF);

    uart->write(END);
}
