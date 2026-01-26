#include "addons/uart_input_addon.h"
#include "storagemanager.h"
#include "drivermanager.h"
#include "hardware/gpio.h"
#include "uart_slip.h"
#include <cstring>
#include <cmath>

#ifndef LED_PIN_DEBUG
#define LED_PIN_DEBUG 25
#endif

inline void debug_blink(int count, int speed_ms) {
    for (int i = 0; i < count; i++) {
        gpio_put(LED_PIN_DEBUG, 1);
        sleep_ms(speed_ms);
        gpio_put(LED_PIN_DEBUG, 0);
        sleep_ms(speed_ms);
    }
}

UARTInputAddon::UARTInputAddon()
    : uart(nullptr),
      isEnabled(false),
      slip(),
      deviceManager(),
      reportProcessor(uartState),
      packetHandler(deviceManager, reportProcessor)
{
    joystickMid = DriverManager::getInstance().getDriver() != nullptr ?
        DriverManager::getInstance().getDriver()->GetJoystickMidValue() : GAMEPAD_JOYSTICK_MID;
}

bool UARTInputAddon::available() {
    const AddonOptions& addonOptions = Storage::getInstance().getAddonOptions();
    const PeripheralOptions& periphOptions = Storage::getInstance().getPeripheralOptions();
    return addonOptions.dualPicoHostOptions.enabled && 
           ((DUAL_UART_BLOCK == 0) ? periphOptions.blockUART0.enabled : periphOptions.blockUART1.enabled);
}

void UARTInputAddon::setup() {
    isEnabled = false;

    gpio_init(LED_PIN_DEBUG);
    gpio_set_dir(LED_PIN_DEBUG, GPIO_OUT);
    gpio_put(LED_PIN_DEBUG, 0);
    
    uart = PeripheralManager::getInstance().getUART(DUAL_UART_BLOCK);
    
    if (!uart || !uart->configured) {
        debug_blink(5, 100);
        return;
    }

    packetHandler.setSendCallback([this](const uint8_t* data, uint16_t len) {
        this->sendPacket(data, len);
    });
    
    slip.reset();
    debug_blink(2, 200);
    isEnabled = true;
}

void UARTInputAddon::reinit() {
    setup();
}

void UARTInputAddon::preprocess() {

    if (!isEnabled || !uart) return;

    bool gotFrame = false;

    while (uart->isReadable()) {
        auto res = slip.push(uart->read());
        if (res == SlipFrameDecoder::Result::FRAME_OK) {
            packetHandler.handlePacket(slip.frameData(), slip.frameSize());
            gotFrame = true;
        }
    }

    Gamepad* gamepad = Storage::getInstance().GetGamepad();
    gamepad->state.buttons |= uartState.buttons;
    gamepad->state.dpad |= uartState.dpad; 

    applyMouse(gamepad);

    applyKeyboardToLeftStick(gamepad);

    uartState.mouse_wheel = 0;
}


void UARTInputAddon::sendPacket(const uint8_t* data, uint16_t len) {
    if (!uart || !uart->isWritable()) return;

    uint32_t crc = slip.calculateCRC32(data, len);

    uart->write(SLIP_END);

    auto sendEscaped = [&](uint8_t b) {
        if (b == SLIP_END) {
            uart->write(SLIP_ESC);
            uart->write(SLIP_ESC_END);
        } else if (b == SLIP_ESC) {
            uart->write(SLIP_ESC);
            uart->write(SLIP_ESC_ESC);
        } else {
            uart->write(b);
        }
    };

    for (uint16_t i = 0; i < len; i++) sendEscaped(data[i]);
    for (int i = 0; i < 4; i++) sendEscaped((crc >> (i * 8)) & 0xFF);

    uart->write(SLIP_END);
}

void UARTInputAddon::applyMouse(Gamepad* gamepad) {
    if (uartState.mouseActive) {
        mouseResetNextTimer = getMillis() + 16;
        uartState.rx = scaleMouseToJoystick(uartState.mouse_dx);
        uartState.ry = scaleMouseToJoystick(uartState.mouse_dy);

        uartState.mouse_dx = 0;
        uartState.mouseActive = false;
    }
    else if (mouseResetNextTimer < getMillis()) {
        uartState.rx = joystickMid;
        uartState.ry = joystickMid;
    }
    gamepad->state.rx = uartState.rx;
    gamepad->state.ry = uartState.ry;

    if (uartState.mouse_wheel < 0) {
        gamepad->state.buttons |= GAMEPAD_MASK_L1;
    }
}

uint16_t UARTInputAddon::scaleMouseToJoystick(int32_t mouseVal) {
    int32_t result = joystickMid + mouseVal * (70 / 10.0f) * MOUSE_SCALE_FACTOR;
    return std::clamp(result, GAMEPAD_JOYSTICK_MIN_I32, GAMEPAD_JOYSTICK_MAX_I32);
}

void UARTInputAddon::applyKeyboardToLeftStick(Gamepad* gamepad) {
    int32_t lx = joystickMid;
    int32_t ly = joystickMid;

    if (uartState.key_w) ly -= 32767; // вверх
    if (uartState.key_s) ly += 32767; // вниз
    if (uartState.key_a) lx -= 32767; // влево
    if (uartState.key_d) lx += 32767; // вправо

    if (uartState.mouse_wheel > 0) {
        ly -= 32767; 
        if (ly < GAMEPAD_JOYSTICK_MIN_I32) ly = GAMEPAD_JOYSTICK_MIN_I32; 
    }

    uartState.lx = lx;
    uartState.ly = ly;
    gamepad->state.lx = lx;
    gamepad->state.ly = ly;
}
