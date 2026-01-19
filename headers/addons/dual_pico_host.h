#ifndef _DUAL_PICO_HOST_H_
#define _DUAL_PICO_HOST_H_

#include "gpaddon.h"
#include "addons/keyboard_host_listener.h" // Для KeyboardButtonMapping

// Настройки по умолчанию (можно переопределить в board_config.h)
#ifndef DUAL_HOST_UART_ID
#define DUAL_HOST_UART_ID uart1  // Важно: меняем uart0 на uart1
#endif

#ifndef DUAL_HOST_PIN_TX
#define DUAL_HOST_PIN_TX 20
#endif

#ifndef DUAL_HOST_PIN_RX
#define DUAL_HOST_PIN_RX 21
#endif

#ifndef DUAL_HOST_PIN_CTS
#define DUAL_HOST_PIN_CTS 26
#endif

#ifndef DUAL_HOST_PIN_RTS
#define DUAL_HOST_PIN_RTS 27
#endif

#ifndef DUAL_HOST_BAUDRATE
#define DUAL_HOST_BAUDRATE 4000000 // Должно совпадать со второй платой
#endif

#define DualPicoHostName "DualPicoHost"

class DualPicoHostAddon : public GPAddon {
public:
    virtual bool available();
    virtual void setup();
    virtual void process();
    virtual void preprocess();
    virtual void postprocess(bool sent) {}
    virtual void reinit();
    virtual std::string name() { return DualPicoHostName; }

private:
    void setup_mappings();
    void process_serial();
    void handle_packet(const uint8_t* data, uint16_t len);
    
    // Обработчики конкретных команд
    void handle_report_received(const uint8_t* data, uint16_t len);
    void handle_device_connected(const uint8_t* data, uint16_t len);
    void handle_device_disconnected(const uint8_t* data, uint16_t len);

    // Логика маппинга (портировано из KeyboardHostListener)
    uint8_t getKeycodeFromModifier(uint8_t modifier);
    void process_kbd_report(const uint8_t* report, uint16_t len);
    void process_mouse_report(const uint8_t* report, uint16_t len);
    uint16_t scaleMouseToJoystick(int8_t mouseVal);

    // UART буфер
    uint8_t rx_buffer[512];
    uint16_t rx_idx;
    bool rx_escaped;

    // Состояние
    GamepadState _host_state;
    
    // Маппинги (аналогично KeyboardHostListener)
    KeyboardButtonMapping _mapDpadUp;
    KeyboardButtonMapping _mapDpadDown;
    KeyboardButtonMapping _mapDpadLeft;
    KeyboardButtonMapping _mapDpadRight;
    KeyboardButtonMapping _mapButtonB1;
    KeyboardButtonMapping _mapButtonB2;
    KeyboardButtonMapping _mapButtonB3;
    KeyboardButtonMapping _mapButtonB4;
    KeyboardButtonMapping _mapButtonL1;
    KeyboardButtonMapping _mapButtonR1;
    KeyboardButtonMapping _mapButtonL2;
    KeyboardButtonMapping _mapButtonR2;
    KeyboardButtonMapping _mapButtonS1;
    KeyboardButtonMapping _mapButtonS2;
    KeyboardButtonMapping _mapButtonL3;
    KeyboardButtonMapping _mapButtonR3;
    KeyboardButtonMapping _mapButtonA1;
    KeyboardButtonMapping _mapButtonA2;
    KeyboardButtonMapping _mapButtonA3;
    KeyboardButtonMapping _mapButtonA4;

    // Параметры мыши
    uint16_t mouseLeftMapping;
    uint16_t mouseMiddleMapping;
    uint16_t mouseRightMapping;
    uint32_t mouseSensitivity;
    float mouseSensitivityScale;
    int16_t joystickMid;
    uint8_t mouseMovementMode;
};

#endif