#include "addons/dual_pico_host.h"
#include "addons/dual_protocol.h"
#include "storagemanager.h"
#include "drivermanager.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include <algorithm>
#include <cstring>

// Вспомогательная функция CRC32 (полином 0xEDB88320)
static uint32_t crc32_pkt(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}

bool DualPicoHostAddon::available() {
    const DualPicoHostOptions& options = Storage::getInstance().getAddonOptions().dualPicoHostOptions;
    return options.enabled;
}

void DualPicoHostAddon::setup() {
    // 1. Инициализация UART1 на скорости 4000000
    uart_init(DUAL_HOST_UART_ID, DUAL_HOST_BAUDRATE);
    
    // 2. Назначение функций GPIO для всех 4 пинов
    gpio_set_function(DUAL_HOST_PIN_TX, GPIO_FUNC_UART);
    gpio_set_function(DUAL_HOST_PIN_RX, GPIO_FUNC_UART);
    gpio_set_function(DUAL_HOST_PIN_CTS, GPIO_FUNC_UART);
    gpio_set_function(DUAL_HOST_PIN_RTS, GPIO_FUNC_UART);

    // 3. Включаем аппаратное управление потоком (CTS/RTS)
    // true, true = включить CTS и включить RTS
    uart_set_hw_flow(DUAL_HOST_UART_ID, true, true);

    // 4. Включаем FIFO (обязательно для высоких скоростей)
    uart_set_fifo_enabled(DUAL_HOST_UART_ID, true);
    uart_set_translate_crlf(DUAL_HOST_UART_ID, false);

    // Сброс внутренних переменных аддона
    rx_idx = 0;
    rx_escaped = false;

    // Загрузка маппингов
    setup_mappings();
    
    // Отправляем B_INIT (сообщаем второй плате, что мы готовы)
    // Структура должна совпадать с b_init_t
    b_init_t msg;
    msg.command = DualCommand::B_INIT;
    msg.interval_override = 0; // default

    // Простая упаковка и отправка (без escaping для простоты, т.к. данные простые)
    // Но по-хорошему нужно реализовать serial_write с CRC и escaping.
    // Пока предположим, что вторая плата ждет пакет. 
    // В данном примере мы сосредоточимся на ПРИЕМЕ.
}

void DualPicoHostAddon::reinit() {
    setup_mappings();
}

void DualPicoHostAddon::setup_mappings() {
    const KeyboardHostOptions& keyboardHostOptions = Storage::getInstance().getAddonOptions().keyboardHostOptions;
    const KeyboardMapping& keyboardMapping = keyboardHostOptions.mapping;

    // Копируем логику из KeyboardHostListener
    _mapDpadUp.setMask(GAMEPAD_MASK_UP); _mapDpadUp.setKey(keyboardMapping.keyDpadUp);
    _mapDpadDown.setMask(GAMEPAD_MASK_DOWN); _mapDpadDown.setKey(keyboardMapping.keyDpadDown);
    _mapDpadLeft.setMask(GAMEPAD_MASK_LEFT); _mapDpadLeft.setKey(keyboardMapping.keyDpadLeft);
    _mapDpadRight.setMask(GAMEPAD_MASK_RIGHT); _mapDpadRight.setKey(keyboardMapping.keyDpadRight);
    
    _mapButtonB1.setMask(GAMEPAD_MASK_B1); _mapButtonB1.setKey(keyboardMapping.keyButtonB1);
    _mapButtonB2.setMask(GAMEPAD_MASK_B2); _mapButtonB2.setKey(keyboardMapping.keyButtonB2);
    _mapButtonB3.setMask(GAMEPAD_MASK_B3); _mapButtonB3.setKey(keyboardMapping.keyButtonB3);
    _mapButtonB4.setMask(GAMEPAD_MASK_B4); _mapButtonB4.setKey(keyboardMapping.keyButtonB4);
    
    _mapButtonL1.setMask(GAMEPAD_MASK_L1); _mapButtonL1.setKey(keyboardMapping.keyButtonL1);
    _mapButtonR1.setMask(GAMEPAD_MASK_R1); _mapButtonR1.setKey(keyboardMapping.keyButtonR1);
    _mapButtonL2.setMask(GAMEPAD_MASK_L2); _mapButtonL2.setKey(keyboardMapping.keyButtonL2);
    _mapButtonR2.setMask(GAMEPAD_MASK_R2); _mapButtonR2.setKey(keyboardMapping.keyButtonR2);
    
    _mapButtonS1.setMask(GAMEPAD_MASK_S1); _mapButtonS1.setKey(keyboardMapping.keyButtonS1);
    _mapButtonS2.setMask(GAMEPAD_MASK_S2); _mapButtonS2.setKey(keyboardMapping.keyButtonS2);
    _mapButtonL3.setMask(GAMEPAD_MASK_L3); _mapButtonL3.setKey(keyboardMapping.keyButtonL3);
    _mapButtonR3.setMask(GAMEPAD_MASK_R3); _mapButtonR3.setKey(keyboardMapping.keyButtonR3);
    
    _mapButtonA1.setMask(GAMEPAD_MASK_A1); _mapButtonA1.setKey(keyboardMapping.keyButtonA1);
    _mapButtonA2.setMask(GAMEPAD_MASK_A2); _mapButtonA2.setKey(keyboardMapping.keyButtonA2);

    mouseLeftMapping = keyboardHostOptions.mouseLeft;
    mouseMiddleMapping = keyboardHostOptions.mouseMiddle;
    mouseRightMapping = keyboardHostOptions.mouseRight;
    mouseSensitivity = keyboardHostOptions.mouseSensitivity;
    mouseMovementMode = keyboardHostOptions.movementMode;
    mouseSensitivityScale = mouseSensitivity / 10.0f;

    joystickMid = DriverManager::getInstance().getDriver() != nullptr ?
        DriverManager::getInstance().getDriver()->GetJoystickMidValue() : GAMEPAD_JOYSTICK_MID;

    // Сброс локального состояния
    _host_state.dpad = 0;
    _host_state.buttons = 0;
    _host_state.lx = joystickMid;
    _host_state.ly = joystickMid;
    _host_state.rx = joystickMid;
    _host_state.ry = joystickMid;
}

void DualPicoHostAddon::process() {
    process_serial();
}

void DualPicoHostAddon::process_serial() {
    // Читаем всё что есть в FIFO UART
    while (uart_is_readable(DUAL_HOST_UART_ID)) {
        uint8_t c = uart_getc(DUAL_HOST_UART_ID);

        if (rx_escaped) {
            switch (c) {
                case SERIAL_ESC_END:
                    rx_buffer[rx_idx++] = SERIAL_END;
                    break;
                case SERIAL_ESC_ESC:
                    rx_buffer[rx_idx++] = SERIAL_ESC;
                    break;
                default:
                    rx_buffer[rx_idx++] = c;
                    break;
            }
            rx_escaped = false;
        } else {
            switch (c) {
                case SERIAL_END:
                    // Конец пакета, проверяем целостность
                    if (rx_idx > 4) { // Минимум команда + 4 байта CRC
                        uint32_t received_crc = 
                            (rx_buffer[rx_idx-4] << 24) | 
                            (rx_buffer[rx_idx-3] << 16) | 
                            (rx_buffer[rx_idx-2] << 8) | 
                            rx_buffer[rx_idx-1];
                        
                        // CRC в HID Remapper передается little-endian в коде, но serial.cc пишет вручную
                        // В serial.cc: (received_crc << 8) | buffer[bytes_read - 1 - i]; 
                        // Это big-endian распаковка из потока. Проверим расчет.
                        // Проще использовать crc32_pkt на полезной нагрузке.
                        
                        uint32_t calc_crc = crc32_pkt(rx_buffer, rx_idx - 4);
                        
                        // HID Remapper Serial.cc пишет CRC "задом наперед" побайтово:
                        // for (int i = 0; i < 4; i++) send((crc >> (i * 8)) & 0xFF);
                        // Значит в потоке лежит LSB first.
                        uint32_t packet_crc = 
                            rx_buffer[rx_idx-4] | 
                            (rx_buffer[rx_idx-3] << 8) | 
                            (rx_buffer[rx_idx-2] << 16) | 
                            (rx_buffer[rx_idx-1] << 24);

                        if (calc_crc == packet_crc) {
                            handle_packet(rx_buffer, rx_idx - 4);
                        }
                    }
                    rx_idx = 0; // Сброс для следующего пакета
                    break;
                case SERIAL_ESC:
                    rx_escaped = true;
                    break;
                default:
                    if (rx_idx < sizeof(rx_buffer)) {
                        rx_buffer[rx_idx++] = c;
                    }
                    break;
            }
        }
    }
}

void DualPicoHostAddon::handle_packet(const uint8_t* data, uint16_t len) {
    if (len == 0) return;

    DualCommand cmd = (DualCommand)data[0];
    switch (cmd) {
        case DualCommand::REPORT_RECEIVED:
            handle_report_received(data, len);
            break;
        case DualCommand::DEVICE_CONNECTED:
            // Можно обрабатывать подключение для логирования
            break;
        default:
            break;
    }
}

void DualPicoHostAddon::handle_report_received(const uint8_t* data, uint16_t len) {
    // report_received_t без поля report[] занимает 3 байта (command, dev_addr, interface)
    if (len < 3) return;
    
    const report_received_t* msg = (const report_received_t*)data;
    uint16_t report_len = len - 3; // Размер самого HID отчета
    const uint8_t* report = msg->report;

    // Эвристика для определения типа устройства (т.к. мы не парсим дескрипторы полноценно)
    // Клавиатура обычно отправляет 8 байт (Modifier, Reserved, Key[6])
    // Мышь обычно 3-8 байт.
    
    // В идеале нужно хранить карту dev_addr -> type на основе DEVICE_CONNECTED.
    // Но для простоты используем проверку длины и содержимого, как это делает простой парсер.
    // Keyboard boot protocol: 8 bytes.
    if (report_len == 8) {
        process_kbd_report(report, report_len);
    } else if (report_len >= 3 && report_len <= 8) {
        // Предполагаем мышь (Buttons, X, Y, [Wheel])
        process_mouse_report(report, report_len);
    }
}

void DualPicoHostAddon::preprocess() {
    // Сбрасываем состояние перед новым циклом обработки ввода, 
    // но сохраняем аналоговые значения в центре
    _host_state.dpad = 0;
    _host_state.buttons = 0;
    // Аналоговые стики не сбрасываем в 0, они перезаписываются при наличии ввода мыши
    // или остаются в центре, если ввода нет (обрабатывается в process_mouse_report)
    
    Gamepad *gamepad = Storage::getInstance().GetGamepad();
    
    // Применяем накопленное состояние к геймпаду
    gamepad->state.dpad |= _host_state.dpad;
    gamepad->state.buttons |= _host_state.buttons;
    
    // Для стиков логика чуть сложнее: если мышь двигалась, обновляем.
    if (_host_state.lx != joystickMid || _host_state.ly != joystickMid) {
        gamepad->state.lx = _host_state.lx;
        gamepad->state.ly = _host_state.ly;
    }
    if (_host_state.rx != joystickMid || _host_state.ry != joystickMid) {
        gamepad->state.rx = _host_state.rx;
        gamepad->state.ry = _host_state.ry;
    }
}

// --- Logic from KeyboardHostListener ---

uint8_t DualPicoHostAddon::getKeycodeFromModifier(uint8_t modifier) {
    switch (modifier) {
      case KEYBOARD_MODIFIER_LEFTCTRL   : return HID_KEY_CONTROL_LEFT ;
      case KEYBOARD_MODIFIER_LEFTSHIFT  : return HID_KEY_SHIFT_LEFT   ;
      case KEYBOARD_MODIFIER_LEFTALT    : return HID_KEY_ALT_LEFT     ;
      case KEYBOARD_MODIFIER_LEFTGUI    : return HID_KEY_GUI_LEFT     ;
      case KEYBOARD_MODIFIER_RIGHTCTRL  : return HID_KEY_CONTROL_RIGHT;
      case KEYBOARD_MODIFIER_RIGHTSHIFT : return HID_KEY_SHIFT_RIGHT  ;
      case KEYBOARD_MODIFIER_RIGHTALT   : return HID_KEY_ALT_RIGHT    ;
      case KEYBOARD_MODIFIER_RIGHTGUI   : return HID_KEY_GUI_RIGHT    ;
    }
    return 0;
}

void DualPicoHostAddon::process_kbd_report(const uint8_t* report, uint16_t len) {
    // report[0] = modifiers, report[1] = reserved, report[2..7] = keycodes
    uint8_t modifier = report[0];
    
    // Проходим по модификаторам и клавишам
    for(uint8_t i=0; i<14; i++) // 6 keys + 8 modifiers
    {
        uint8_t keycode = 0;
        if (i < 6) {
            keycode = report[2 + i];
        } else {
            // Check modifier bits
            uint8_t bit = i - 6;
            if (modifier & (1 << bit)) {
                 keycode = getKeycodeFromModifier(1 << bit);
            }
        }

        if (keycode) {
            // Маппинг на D-Pad
            if (keycode == _mapDpadUp.key)    _host_state.dpad |= _mapDpadUp.buttonMask;
            if (keycode == _mapDpadDown.key)  _host_state.dpad |= _mapDpadDown.buttonMask;
            if (keycode == _mapDpadLeft.key)  _host_state.dpad |= _mapDpadLeft.buttonMask;
            if (keycode == _mapDpadRight.key) _host_state.dpad |= _mapDpadRight.buttonMask;

            // Маппинг на кнопки
            if (keycode == _mapButtonB1.key)  _host_state.buttons |= _mapButtonB1.buttonMask;
            if (keycode == _mapButtonB2.key)  _host_state.buttons |= _mapButtonB2.buttonMask;
            if (keycode == _mapButtonB3.key)  _host_state.buttons |= _mapButtonB3.buttonMask;
            if (keycode == _mapButtonB4.key)  _host_state.buttons |= _mapButtonB4.buttonMask;
            
            if (keycode == _mapButtonL1.key)  _host_state.buttons |= _mapButtonL1.buttonMask;
            if (keycode == _mapButtonR1.key)  _host_state.buttons |= _mapButtonR1.buttonMask;
            if (keycode == _mapButtonL2.key)  _host_state.buttons |= _mapButtonL2.buttonMask;
            if (keycode == _mapButtonR2.key)  _host_state.buttons |= _mapButtonR2.buttonMask;
            
            if (keycode == _mapButtonS1.key)  _host_state.buttons |= _mapButtonS1.buttonMask;
            if (keycode == _mapButtonS2.key)  _host_state.buttons |= _mapButtonS2.buttonMask;
            if (keycode == _mapButtonL3.key)  _host_state.buttons |= _mapButtonL3.buttonMask;
            if (keycode == _mapButtonR3.key)  _host_state.buttons |= _mapButtonR3.buttonMask;
            
            if (keycode == _mapButtonA1.key)  _host_state.buttons |= _mapButtonA1.buttonMask;
            if (keycode == _mapButtonA2.key)  _host_state.buttons |= _mapButtonA2.buttonMask;
        }
    }
}

uint16_t DualPicoHostAddon::scaleMouseToJoystick(int8_t mouseVal) {
    // MOUSE_SCALE_FACTOR определен в keyboard_host_listener.cpp, но недоступен здесь.
    // Переопределим:
    #define LOCAL_MOUSE_SCALE_FACTOR (GAMEPAD_JOYSTICK_MID / 127)
    int32_t result = joystickMid + (int32_t)mouseVal * mouseSensitivityScale * LOCAL_MOUSE_SCALE_FACTOR;
    return std::clamp(result, (int32_t)GAMEPAD_JOYSTICK_MIN, (int32_t)GAMEPAD_JOYSTICK_MAX);
}

void DualPicoHostAddon::process_mouse_report(const uint8_t* report, uint16_t len) {
    // Boot mouse report:
    // Byte 0: Buttons (Bit 0: Left, 1: Right, 2: Middle)
    // Byte 1: X displacement
    // Byte 2: Y displacement
    // Byte 3: Optional Wheel
    
    uint8_t buttons = report[0];
    int8_t x = (int8_t)report[1];
    int8_t y = (int8_t)report[2];
    
    _host_state.buttons |=
        (buttons & MOUSE_BUTTON_LEFT   ? mouseLeftMapping : 0)
      | (buttons & MOUSE_BUTTON_MIDDLE ? mouseMiddleMapping : 0)
      | (buttons & MOUSE_BUTTON_RIGHT  ? mouseRightMapping : 0);

    if (mouseMovementMode == MOUSE_MOVEMENT_LEFT_ANALOG) {
        _host_state.lx = scaleMouseToJoystick(x);
        _host_state.ly = scaleMouseToJoystick(y);
    } else if (mouseMovementMode == MOUSE_MOVEMENT_RIGHT_ANALOG) {
        _host_state.rx = scaleMouseToJoystick(x);
        _host_state.ry = scaleMouseToJoystick(y);
    }
}