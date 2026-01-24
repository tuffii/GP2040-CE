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
    switch (usage) {

        // ===== Keyboard =====
        case 0x07002C: // Space
            if (value)
                uartState.buttons |= GAMEPAD_MASK_B1;
            else
                uartState.buttons &= ~GAMEPAD_MASK_B1;
            break;

        // ===== Mouse buttons =====
        case 0x090001: // Left
            if (value)
                uartState.buttons |= GAMEPAD_MASK_B2;
            else
                uartState.buttons &= ~GAMEPAD_MASK_B2;
            break;

        // ===== Mouse axes (SIGNED) =====
        case 0x010030: // X
            if (value != 0) {
                uartState.mouse_dx = value;
                uartState.mouseActive = true;
            }
            break;
        case 0x010031: // Y
            if (value != 0) {
                uartState.mouse_dy = value;
                uartState.mouseActive = true;
            }
            break;
        case 0x010038: // Wheel
            uartState.mouse_wheel += value;
            break;

        default:
            break;
    }
}
