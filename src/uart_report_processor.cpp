#include "uart_report_processor.h"
#include "storagemanager.h"
#include "pico/stdlib.h"
#include <cstring>
#include <algorithm>

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

UARTReportProcessor::UARTReportProcessor(UARTInputState& state)
    : uartState(state) {
    }

void UARTReportProcessor::processReport(UARTDeviceContext& device, const uint8_t* report, size_t len) {
    if (!report || !device.active || len == 0) return;

    uint8_t report_id = 0;
    const uint8_t* data = report;
    size_t data_len = len;

    if (device.hasReportId) {
        report_id = report[0];
        if (len <= 1) return;
        data = report + 1;
        data_len = len - 1;
    }

    auto it = device.usages.find(report_id);
    if (it == device.usages.end()) return;

    for (auto& [usage, def] : it->second) {
        int32_t value = extractValue(data, data_len, def, usage);
        applyUsageToState(usage, def, value);
    }
}


int32_t UARTReportProcessor::extractValue(
    const uint8_t* report,
    size_t len,
    usage_def_t& def,
    uint32_t target_usage
) {
    if (!report) return 0;

    // ===== Array (keyboard, etc) =====
    if (def.is_array) {
        uint32_t byte_offset = def.bitpos / 8;
        uint8_t search_value = target_usage & 0xFF;

        if (byte_offset + def.count > len) return 0;

        for (uint32_t i = 0; i < def.count; i++) {
            if (report[byte_offset + i] == search_value) {
                return 1;
            }
        }
        return 0;
    }

    // ===== Variable =====
    uint32_t bit_offset = def.bitpos;
    uint32_t bit_count  = def.size;

    if ((bit_offset + bit_count) > (len * 8)) {
        return 0;
    }

    int32_t value = 0;

    // Извлечение битов (LSB first, HID standard)
    for (uint32_t i = 0; i < bit_count; i++) {
        uint32_t bit = bit_offset + i;
        uint32_t byte_idx = bit / 8;
        uint32_t bit_idx  = bit % 8;

        if (report[byte_idx] & (1 << bit_idx)) {
            value |= (1 << i);
        }
    }

    // ===== SIGN EXTENSION =====
    // HID: если logical_min < 0 — значение signed
    if (def.logical_minimum < 0) {
        int32_t sign_bit = 1 << (bit_count - 1);
        if (value & sign_bit) {
            value |= ~((1 << bit_count) - 1);
        }
    }

    // ===== Clamping (на всякий случай) =====
    if (value > def.logical_maximum) value = def.logical_maximum;
    if (value < def.logical_minimum) value = def.logical_minimum;

    // ===== Relative handling =====
    if (def.is_relative) {
        // для мыши возвращаем дельту как есть (signed!)
        return value;
    }

    // ===== Absolute =====
    def.current_value = value;
    def.has_value = true;
    return value;
}


void UARTReportProcessor::updateUsageState(usage_def_t& usage, int32_t value) {
    // Этот метод теперь можно удалить или упростить, так как состояние в структуре
    usage.current_value = value;
    usage.has_value = true;
}

void UARTReportProcessor::applyUsageToState(uint32_t usage, const usage_def_t& def, int32_t value) {
    auto handleButton = [&](uint16_t mask) {
        if (value) {
            uartState.buttons |= mask;
            gpio_put(LED_PIN_DEBUG, 1);
        } else {
            uartState.buttons &= ~mask;
            gpio_put(LED_PIN_DEBUG, 0);
        }
    };

    auto handleDpad = [&](uint8_t mask) {
        if (value) {
            uartState.dpad |= mask;
            gpio_put(LED_PIN_DEBUG, 1);
        } else {
            uartState.dpad &= ~mask;
            gpio_put(LED_PIN_DEBUG, 0);
        }
    };

    switch (usage) {
        // ============================
        //          MOUSE
        // ============================
        case 0x00090001: // ЛКМ -> RB
            handleButton(GAMEPAD_MASK_R2);
            break;
        case 0x00090003: // Средняя кнопка -> RB
            handleButton(GAMEPAD_MASK_R1);
            break;
        case 0x00090002: // ПКМ -> LT
            handleButton(GAMEPAD_MASK_L2);
            break;

        // ============================
        //          KEYBOARD
        // ============================
        
        // --- Основные кнопки ---
        case 0x070014: // Q -> A
            handleButton(GAMEPAD_MASK_B1);
            break;
            
        case 0x070020: // 3 -> Y
            handleButton(GAMEPAD_MASK_B4);
            break;

        case 0x070015: // R -> X
            handleButton(GAMEPAD_MASK_B3);
            break;
        case 0x070008: // E -> X
            handleButton(GAMEPAD_MASK_B3);
            break;

        case 0x0700E0: // LCtrl -> B
            handleButton(GAMEPAD_MASK_B2);
            break;
            
        case 0x07002C: // Space -> LB
            handleButton(GAMEPAD_MASK_L1);
            break;

        case 0x0700E1: // LShift -> L3 (Нажатие левого стика)
            handleButton(GAMEPAD_MASK_L3);
            break;

        case 0x070019: // V -> R3 (Нажатие правого стика)
            handleButton(GAMEPAD_MASK_R3);
            break;

        // --- Левая крестовина (D-Pad) ---
        case 0x070021: // 4 -> Вверх
            handleDpad(GAMEPAD_MASK_UP);
            break;
            
        case 0x07003A: // F1 -> Вниз
            handleDpad(GAMEPAD_MASK_DOWN);
            break;

        case 0x070005: // B -> Влево
            handleDpad(GAMEPAD_MASK_LEFT);
            break;

        case 0x07000A: // G -> Вправо
            handleDpad(GAMEPAD_MASK_RIGHT);
            break;

        // --- WASD (Движение) ---
        case 0x0007001A: // W
            uartState.key_w = value;
            gpio_put(LED_PIN_DEBUG, value ? 1 : 0);
            break;
        case 0x00070016: // S
            uartState.key_s = value;
            gpio_put(LED_PIN_DEBUG, value ? 1 : 0);
            break;
        case 0x00070004: // A
            uartState.key_a = value;
            gpio_put(LED_PIN_DEBUG, value ? 1 : 0);
            break;
        case 0x00070007: // D
            uartState.key_d = value;
            gpio_put(LED_PIN_DEBUG, value ? 1 : 0);
            break;

        // ============================
        //        MOUSE AXES
        // ============================
        case 0x010030: // X Axis
            uartState.mouse_dx = value;
            uartState.mouseActive = true;
            break;
        case 0x010031: // Y Axis
            uartState.mouse_dy = value;
            uartState.mouseActive = true;
            break;
        case 0x010038: // Wheel
            uartState.mouse_wheel += value;
            break;

        default:
            break;
    }
}
