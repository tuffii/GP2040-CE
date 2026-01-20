#ifndef _PERIPHERAL_UART_H_
#define _PERIPHERAL_UART_H_

#include <hardware/uart.h>
#include <hardware/gpio.h>
#include <hardware/platform_defs.h>

#ifndef NUM_UARTS
#define NUM_UARTS 2
#endif

#ifndef UART_DEFAULT_BAUDRATE
#define UART_DEFAULT_BAUDRATE 4000000
#endif

class PeripheralUART {
public:
    PeripheralUART();
    ~PeripheralUART() {}

    bool configured = false;

    void setConfig(uint8_t block, int8_t tx, int8_t rx, int8_t cts, int8_t rts, uint32_t speed = UART_DEFAULT_BAUDRATE);

    bool isWritable();
    bool isReadable();
    
    void write(uint8_t data);
    void write(const uint8_t* data, uint32_t len);
    
    uint8_t read();
    
    uart_inst_t* getDriver() { return _UART; }

private:
    uart_inst_t* _UART;

    int8_t _TX;
    int8_t _RX;
    int8_t _CTS;
    int8_t _RTS;
    uint32_t _Speed;

    void setup();
};

#endif