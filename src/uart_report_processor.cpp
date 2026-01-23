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
    : uartState(state) {}

// Обрабатываем новый HID-отчёт
void UARTReportProcessor::processReport(
    UARTDeviceContext& device,
    const uint8_t* report,
    size_t len
) {
    if (!report || !device.active) return;

    for (auto& [report_id, usageMap] : device.usages) {
        for (auto& [usage, def] : usageMap) {

            int32_t value = extractValue(report, len, def);

            bool pressed = (value != 0);

            applyUsageToState(usage, def, pressed);
        }
    }
}


// Извлекаем значение usage из отчёта
int32_t UARTReportProcessor::extractValue(const uint8_t* report, size_t len, const usage_def_t& usage) {
    if (!report || usage.bitpos / 8 >= len) return 0;

    uint32_t bitOffset = usage.bitpos;
    uint32_t value = 0;

    // Собираем значение по битам
    for (uint8_t i = 0; i < usage.size; ++i) {
        uint32_t byteIdx = (bitOffset + i) / 8;
        uint32_t bitIdx  = (bitOffset + i) % 8;
        if (byteIdx >= len) break;

        if (report[byteIdx] & (1 << bitIdx)) {
            value |= 1 << i;
        }
    }

    // Масштабирование и учёт логических границ
    if (usage.should_be_scaled) {
        if (value > usage.logical_maximum) value = usage.logical_maximum;
        if (value < usage.logical_minimum) value = usage.logical_minimum;
    }

    // Учёт относительных значений
    if (usage.is_relative) {
        // value будет суммироваться с прошлым состоянием
        if (usage.input_state_0) value += *(usage.input_state_0);
    }

    return static_cast<int32_t>(value);
}

// Обновление текущего и предыдущего состояния usage
void UARTReportProcessor::updateUsageState(usage_def_t& usage, int32_t value) {
    if (!usage.input_state_0) {
        // Инициализация при первом использовании
        usage.input_state_n = new int32_t(0);
        usage.input_state_0 = new int32_t(value);
    } else {
        // Сохраняем прошлое состояние
        *(usage.input_state_n) = *(usage.input_state_0);
        *(usage.input_state_0) = value;
    }
}

// Генерация события по новому значению (оставлено для заполнения)
void UARTReportProcessor::applyUsageToState(
    uint32_t usage,
    const usage_def_t& def,
    bool pressed
) {
    uint32_t mask = 0;

    // def.bitpos, def.is_relative

    switch (usage) {
        case 0x07002C: // Keyboard Space
            mask = GAMEPAD_MASK_B1;
            break;

        case 0x090001: // Mouse Left
            mask = GAMEPAD_MASK_B2;
            break;

        default:
            return;
    }

    if (pressed) {
        uartState.buttons |= mask;
    } else {
        uartState.buttons &= ~mask;
    }
}

