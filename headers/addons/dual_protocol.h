#ifndef _DUAL_PROTOCOL_H_
#define _DUAL_PROTOCOL_H_

#include <stdint.h>

// Команды протокола
enum class DualCommand : uint8_t {
    DEVICE_CONNECTED = 1,
    DEVICE_DISCONNECTED = 2,
    REPORT_RECEIVED = 3,
    REQUEST_B_INIT = 4,
    B_INIT = 5,
    RESTART = 6,
    SEND_OUT_REPORT = 7,
    START_OF_FRAME = 8,
    SET_FEATURE_REPORT = 9,
    GET_FEATURE_REPORT = 10,
    GET_FEATURE_RESPONSE = 11,
    SET_FEATURE_COMPLETE = 12,
    MIDI_RECEIVED = 13,
};

// Байты управления потоком (SLIP-like)
#define SERIAL_END 0300
#define SERIAL_ESC 0333
#define SERIAL_ESC_END 0334
#define SERIAL_ESC_ESC 0335

// Структуры данных
struct __attribute__((packed)) device_connected_t {
    DualCommand command;
    uint16_t vid;
    uint16_t pid;
    uint8_t dev_addr;
    uint8_t interface;
    uint8_t hub_port;
    uint8_t itf_num;
    uint8_t report_descriptor[0];
};

struct __attribute__((packed)) device_disconnected_t {
    DualCommand command;
    uint8_t dev_addr;
    uint8_t interface;
};

struct __attribute__((packed)) report_received_t {
    DualCommand command;
    uint8_t dev_addr;
    uint8_t interface;
    uint8_t report[0];
};

struct __attribute__((packed)) request_b_init_t {
    DualCommand command;
};

struct __attribute__((packed)) b_init_t {
    DualCommand command;
    uint8_t interval_override;
};

#endif