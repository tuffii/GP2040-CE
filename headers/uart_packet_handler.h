#ifndef _UART_PACKET_HANDLER_H_
#define _UART_PACKET_HANDLER_H_

#include "uart_protocol.h"
#include "uart_device_manager.h"
#include "uart_report_processor.h"
#include <cstdint>
#include <functional>

class UARTPacketHandler {
public:
    UARTPacketHandler(
        UARTDeviceManager& dm,
        UARTReportProcessor& rp
    );

    void setSendCallback(
        std::function<void(const uint8_t*, uint16_t)> cb
    );

    void handlePacket(const uint8_t* data, uint16_t len);

private:
    UARTDeviceManager& deviceManager;
    UARTReportProcessor& reportProcessor;

    std::function<void(const uint8_t*, uint16_t)> sendCallback;

    void handleDeviceConnected(const device_connected_t* pkt, uint16_t len);
    void handleDeviceDisconnected(const device_disconnected_t* pkt);
    void handleReportReceived(const report_received_t* pkt, uint16_t len);
    void handleRequestBInit(const request_b_init_t* pkt);
};

#endif
