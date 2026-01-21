#include "uart_packet_handler.h"
#include "hardware/gpio.h"
#include <cstring>

#ifndef LED_PIN_DEBUG
#define LED_PIN_DEBUG 25
#endif

void UARTPacketHandler::handlePacket(const uint8_t* data, uint16_t len) {
    if (len == 0) return;

    DualCommand cmd = static_cast<DualCommand>(data[0]);

    switch (cmd) {
        case DualCommand::DEVICE_CONNECTED:
            if (len >= sizeof(device_connected_t)) {
                handleDeviceConnected(reinterpret_cast<const device_connected_t*>(data));
            }
            break;

        case DualCommand::DEVICE_DISCONNECTED:
            if (len >= sizeof(device_disconnected_t)) {
                handleDeviceDisconnected(reinterpret_cast<const device_disconnected_t*>(data));
            }
            break;

        case DualCommand::REPORT_RECEIVED:
            if (len >= sizeof(report_received_t)) {
                handleReportReceived(reinterpret_cast<const report_received_t*>(data));
            }
            break;

        case DualCommand::REQUEST_B_INIT:
            if (len >= sizeof(request_b_init_t)) {
                handleRequestBInit(reinterpret_cast<const request_b_init_t*>(data));
            }
            break;

        default:
            // Игнорируем неизвестные команды
            break;
    }
}

void UARTPacketHandler::handleDeviceConnected(const device_connected_t* pkt) {
    gpio_put(LED_PIN_DEBUG, 1);
}

void UARTPacketHandler::handleDeviceDisconnected(const device_disconnected_t* pkt) {
    gpio_put(LED_PIN_DEBUG, 0);
}

void UARTPacketHandler::handleReportReceived(const report_received_t* pkt) {
    // Тут можно вызвать внешний callback или обновить глобальное состояние
}

void UARTPacketHandler::handleRequestBInit(const request_b_init_t* pkt) {
    if (!sendCallback) return;

    b_init_t resp;
    resp.command = DualCommand::B_INIT;
    resp.interval_override = 0;

    sendCallback(reinterpret_cast<const uint8_t*>(&resp), sizeof(resp));
}
