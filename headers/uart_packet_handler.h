#ifndef _UART_PACKET_HANDLER_H_
#define _UART_PACKET_HANDLER_H_

#include "uart_header.h"
#include <cstdint>
#include <functional>

class UARTPacketHandler {
public:
    UARTPacketHandler() = default;

    void setSendCallback(std::function<void(const uint8_t*, uint16_t)> cb) {
        sendCallback = cb;
    }

    void handlePacket(const uint8_t* data, uint16_t len);

private:
    std::function<void(const uint8_t*, uint16_t)> sendCallback;

    void handleDeviceConnected(const device_connected_t* pkt);
    void handleDeviceDisconnected(const device_disconnected_t* pkt);
    void handleReportReceived(const report_received_t* pkt);
    void handleRequestBInit(const request_b_init_t* pkt);
};

#endif
