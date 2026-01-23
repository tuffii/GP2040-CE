#ifndef _UART_DEVICE_MANAGER_H_
#define _UART_DEVICE_MANAGER_H_

#include "uart_protocol.h"
#include "uart_device_context.h"
#include "uart_descriptor_parser.h"

class UARTDeviceManager {
public:
    UARTDeviceManager();

    void deviceConnected(const device_connected_t* pkt, const uint8_t* reportDescriptor, uint16_t descriptorLen);
    void deviceDisconnected(const device_disconnected_t* pkt);

    UARTDeviceContext* getDevice(uint8_t dev_addr, uint8_t interface);

private:
    static constexpr int MAX_DEVICES = 8;
    UARTDeviceContext devices[MAX_DEVICES];

    UARTDescriptorParser parser;
};

#endif
