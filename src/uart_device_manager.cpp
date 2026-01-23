#include "uart_device_manager.h"
#include <cstring>
#include "uart_descriptor_parser.h"

UARTDeviceManager::UARTDeviceManager() {
    for (int i = 0; i < MAX_DEVICES; ++i) {
        devices[i].active = false;
    }
}

UARTDeviceContext* UARTDeviceManager::getDevice(uint8_t dev_addr, uint8_t interface) {
    for (int i = 0; i < MAX_DEVICES; ++i) {
        if (devices[i].active &&
            devices[i].dev_addr == dev_addr &&
            devices[i].interface == interface) {
            return &devices[i];
        }
    }
    return nullptr;
}

void UARTDeviceManager::deviceConnected(const device_connected_t* pkt, const uint8_t* reportDescriptor, uint16_t descriptorLen) {
    for (int i = 0; i < MAX_DEVICES; ++i) {
        if (!devices[i].active) {
            UARTDeviceContext& dev = devices[i];

            dev.dev_addr = pkt->dev_addr;
            dev.interface = pkt->interface;
            dev.vid = pkt->vid;
            dev.pid = pkt->pid;
            dev.active = true;

            dev.usages.clear();

            parser.parseDeviceDescriptor(dev, reportDescriptor, descriptorLen);
            // apply_quirks(dev.vid,dev.pid,dev.inputUsages,reportDescriptor,descriptorLen,pkt->interface);
            // add_synthetic_dpad_usages(dev.inputUsages);
            return;
        }
    }
}

void UARTDeviceManager::deviceDisconnected(const device_disconnected_t* pkt) {
    for (int i = 0; i < MAX_DEVICES; ++i) {
        if (devices[i].active && devices[i].dev_addr == pkt->dev_addr && devices[i].interface == pkt->interface) {
            devices[i].active = false;
            devices[i].usages.clear();
            return;
        }
    }
}
