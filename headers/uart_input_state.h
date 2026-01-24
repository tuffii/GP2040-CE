#ifndef _UART_INPUT_STATE_H_
#define _UART_INPUT_STATE_H_
#include <cstdint>

#define GAMEPAD_JOYSTICK_MID 0x7FFF
#define MOUSE_SCALE_FACTOR (GAMEPAD_JOYSTICK_MID / 127)
#define GAMEPAD_JOYSTICK_MIN_I32 static_cast<int32_t>(GAMEPAD_JOYSTICK_MIN)
#define GAMEPAD_JOYSTICK_MAX_I32 static_cast<int32_t>(GAMEPAD_JOYSTICK_MAX)

#define UART_MOUSE_MOVE_LEFT_ANALOG 0
#define UART_MOUSE_MOVE_RIGHT_ANALOG 1

#define UART_MOUSE_RESET_MS 500

struct UARTInputState {
    uint32_t buttons = 0;
    uint8_t dpad = 0;
    int16_t lx = GAMEPAD_JOYSTICK_MID;
    int16_t ly = GAMEPAD_JOYSTICK_MID;
    int16_t rx = GAMEPAD_JOYSTICK_MID;
    int16_t ry = GAMEPAD_JOYSTICK_MID;

    int32_t mouse_dx = 0;
    int32_t mouse_dy = 0;
    int32_t mouse_wheel = 0;

    bool mouseActive = false;
};

#endif
