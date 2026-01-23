#include "uart_packet_handler.h"
#include "hardware/gpio.h"
#include <cstring>
#include <cstdint>
#include "pico/stdlib.h"

#ifndef LED_PIN_DEBUG
#define LED_PIN_DEBUG 25
#endif

inline void debug_blink(int count, int speed_ms) {
    for (int i = 0; i < count; i++) {
        gpio_put(LED_PIN_DEBUG, 1);
        sleep_ms(speed_ms);
        gpio_put(LED_PIN_DEBUG, 0);
        sleep_ms(speed_ms);
    }
}

UARTPacketHandler::UARTPacketHandler(UARTDeviceManager& dm, UARTReportProcessor& rp):
    deviceManager(dm),
    reportProcessor(rp)
{}

void UARTPacketHandler::setSendCallback(std::function<void(const uint8_t*, uint16_t)> cb) {
    sendCallback = cb;
}

void UARTPacketHandler::handlePacket(const uint8_t* data, uint16_t len) {
    if (!data || len < 1) {
        return;
    }

    DualCommand cmd = static_cast<DualCommand>(data[0]);

    switch (cmd) {
        case DualCommand::DEVICE_CONNECTED:
            if (len >= sizeof(device_connected_t)) {
                handleDeviceConnected(reinterpret_cast<const device_connected_t*>(data), len);
            }
            break;

        case DualCommand::DEVICE_DISCONNECTED:
            if (len >= sizeof(device_disconnected_t)) {
                handleDeviceDisconnected(reinterpret_cast<const device_disconnected_t*>(data));
            }
            break;

        case DualCommand::REPORT_RECEIVED:
            if (len >= sizeof(report_received_t)) {
                handleReportReceived(reinterpret_cast<const report_received_t*>(data), len);
            }
            break;

        case DualCommand::REQUEST_B_INIT:
            if (len >= sizeof(request_b_init_t)) {
                handleRequestBInit(reinterpret_cast<const request_b_init_t*>(data));
            }
            break;

        default:
            break;
    }
}

void UARTPacketHandler::handleDeviceConnected(const device_connected_t* pkt, uint16_t len) {
    size_t descLen = len - sizeof(device_connected_t);
    const uint8_t* descriptor = pkt->report_descriptor;
    deviceManager.deviceConnected(pkt, descriptor, descLen);
    debug_blink(1, 70);
}

void UARTPacketHandler::handleDeviceDisconnected(const device_disconnected_t* pkt) {
    deviceManager.deviceDisconnected(pkt);
    debug_blink(2, 70);
}

void UARTPacketHandler::handleReportReceived(const report_received_t* pkt,  uint16_t len) {
    UARTDeviceContext* dev = deviceManager.getDevice(pkt->dev_addr, pkt->interface);

    if (!dev) {
        debug_blink(5, 70); // report от неизвестного устройства
        return;
    }

    size_t reportLen = len - sizeof(report_received_t);
    const uint8_t* report = pkt->report;

    reportProcessor.processReport(*dev, report, reportLen);
}

void UARTPacketHandler::handleRequestBInit(const request_b_init_t* pkt) {
    if (!sendCallback) return;

    b_init_t resp;
    resp.command = DualCommand::B_INIT;
    resp.interval_override = 0;

    sendCallback(reinterpret_cast<const uint8_t*>(&resp), sizeof(resp));
}
