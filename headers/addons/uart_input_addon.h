#pragma once

#include "gpaddon.h"
#include "peripheralmanager.h"
#include "uart_packet_handler.h"
#include "uart_slip_frame_decoder.h"
#include "uart_header.h"
#include <cstdint>
#include <string>

#ifndef DUAL_UART_BLOCK
#define DUAL_UART_BLOCK 1
#endif

class UARTInputAddon : public GPAddon {
public:
    UARTInputAddon(): uart(nullptr), isEnabled(false), slip(), handler() {};

    virtual bool available() override;
    virtual void setup() override;
    virtual void preprocess() override;
    virtual void process() override {}
    virtual void postprocess(bool) override {}
    virtual void reinit() override;

    virtual std::string name() override { return "DualPicoHost"; }

private:
    PeripheralUART* uart;           // UART, который мы используем
    bool isEnabled;                 // Флаг успешной инициализации

    SlipFrameDecoder slip;          // SLIP-декодер
    UARTPacketHandler handler;      // Обработчик готовых пакетов

    // Отправка пакета через UART с SLIP + CRC
    void sendPacket(const uint8_t* data, uint16_t len);
};
