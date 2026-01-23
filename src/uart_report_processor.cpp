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
void UARTReportProcessor::processReport(UARTDeviceContext& device, const uint8_t* report, size_t len) {
    if (!report || !device.active) return;

    for (auto& [report_id, usageMap] : device.usages) {
        // Проверка Report ID (если используется)
        // Если устройство использует Report ID, первый байт отчета может быть ID.
        // Обычно hid_remapper обрабатывает смещение данных до вызова processReport или внутри.
        // Предположим, что report уже указывает на данные (или обрабатывается корректно).
        
        for (auto& [usage, def] : usageMap) {
            // ПЕРЕДАЕМ usage (сам ID клавиши) в extractValue
            int32_t value = extractValue(report, len, def, usage);
            
            bool pressed = (value != 0);
            applyUsageToState(usage, def, pressed);
        }
    }
}


// Извлекаем значение usage из отчёта
int32_t UARTReportProcessor::extractValue(const uint8_t* report, size_t len, const usage_def_t& def, uint32_t target_usage) {
    if (!report || def.bitpos / 8 >= len) return 0;

    if (def.is_array) {
        uint32_t byte_offset = def.bitpos / 8;
        uint8_t search_value = target_usage & 0xFF;
        for (uint32_t i = 0; i < def.count; i++) {
            if (byte_offset + i >= len) break;

            if (report[byte_offset + i] == search_value) {
                return 1;
            }
        }
        return 0;
    }

    uint32_t bitOffset = def.bitpos;
    uint32_t value = 0;

    // Собираем значение по битам
    for (uint8_t i = 0; i < def.size; ++i) {
        uint32_t byteIdx = (bitOffset + i) / 8;
        uint32_t bitIdx  = (bitOffset + i) % 8;
        if (byteIdx >= len) break;

        if (report[byteIdx] & (1 << bitIdx)) {
            value |= 1 << i;
        }
    }

    // Масштабирование и учёт логических границ
    if (def.should_be_scaled) {
        if (value > def.logical_maximum) value = def.logical_maximum;
        if (value < def.logical_minimum) value = def.logical_minimum;
    }

    // Учёт относительных значений
    if (def.is_relative) {
        // value будет суммироваться с прошлым состоянием
        if (def.input_state_0) value += *(def.input_state_0);
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

    // if (def.is_array) {
    //     debug_blink(1, 15);
    // }

    switch (usage) {
        case 0x07002C: // Keyboard Space
            debug_blink(1, 15);
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

