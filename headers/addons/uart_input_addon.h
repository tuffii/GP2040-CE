#ifndef _UART_INPUT_ADDON_H_
#define _UART_INPUT_ADDON_H_

#include "gpaddon.h"
#include "peripheralmanager.h"
#include "uart_slip_frame_decoder.h"
#include "uart_packet_handler.h"
#include "uart_device_manager.h"
#include "uart_report_processor.h"
#include "uart_input_state.h"
#include <cstdint>
#include <string>

#ifndef DUAL_UART_BLOCK
#define DUAL_UART_BLOCK 1
#endif

class UARTInputAddon : public GPAddon {
public:
    UARTInputAddon();

    virtual bool available() override;
    virtual void setup() override;
    virtual void preprocess() override;
    virtual void process() override {}
    virtual void postprocess(bool) override {}
    virtual void reinit() override;

    virtual std::string name() override { return "DualPicoHost"; }
private:

    void applyMouse(Gamepad* gamepad);

    PeripheralUART* uart;
    bool isEnabled;

    SlipFrameDecoder slip;

    UARTInputState uartState;

    UARTDeviceManager deviceManager;
    UARTReportProcessor reportProcessor;
    UARTPacketHandler packetHandler;

    uint16_t scaleMouseToJoystick(int32_t mouseVal);

    void sendPacket(const uint8_t* data, uint16_t len);

    void applyKeyboardToLeftStick(Gamepad* gamepad);

    int16_t joystickMid;
    uint32_t mouseResetNextTimer = 0;
};

#endif
