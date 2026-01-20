#ifndef _UART_INPUT_ADDON_H_
#define _UART_INPUT_ADDON_H_

#include "gpaddon.h"
#include "peripheralmanager.h"
#include "addons/uart_header.h"

#ifndef DUAL_UART_BLOCK
#define DUAL_UART_BLOCK 1
#endif

class UARTInputAddon : public GPAddon {
public:
    UARTInputAddon() : uart(nullptr), isEnabled(false) {} // Инициализация!

    virtual bool available();
    virtual void setup();
    virtual void preprocess();
    virtual void process();
    virtual void postprocess(bool) {}
    virtual void reinit();
    virtual std::string name() { return "DualPicoHost"; }

private:
    PeripheralUART* uart;
    bool isEnabled; // Флаг успешной инициализации

    // Буферы для SLIP
    static const size_t BUFFER_SIZE = 512;
    uint8_t rxBuffer[BUFFER_SIZE];
    uint16_t rxIndex = 0;
    bool escaped = false;

    void readUart();
    void handlePacket(const uint8_t* data, uint16_t len);
    void sendPacket(const uint8_t* data, uint16_t len);
    
    void handleRequestBInit();
    void handleReportReceived(const uint8_t* data, uint16_t len);
    void handleDeviceConnected(const uint8_t* data, uint16_t len);
    void handleDeviceDisconnected(const uint8_t* data, uint16_t len);
    
    uint32_t calculateCRC32(const uint8_t* buf, int len);
    void resetState();
    
    static const uint32_t crc_table[256];
};

#endif