#ifndef _UART_INPUT_STATE_H_
#define _UART_INPUT_STATE_H_
#include <cstdint>

struct UARTInputState {
    uint32_t buttons = 0;
    uint8_t dpad = 0;
    int16_t lx = 0;
    int16_t ly = 0;
    int16_t rx = 0;
    int16_t ry = 0;
};

#endif
