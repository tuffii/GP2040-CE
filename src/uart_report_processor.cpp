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

void UARTReportProcessor::processReport(UARTDeviceContext& device, const uint8_t* report, size_t len) {
    if (!report || !device.active || len == 0) return;

    uint8_t incomingReportId = 0;
    const uint8_t* data = report;
    size_t dataLen = len;

    // 1. Логика Report ID
    if (device.hasReportId) {
        incomingReportId = report[0];
        if (len > 1) {
            data = report + 1;
            dataLen = len - 1;
        } else {
            return; 
        }
    }

    // 2. Ищем usages только для текущего Report ID
    auto it = device.usages.find(incomingReportId);
    if (it == device.usages.end()) {
        return; 
    }

    // ВАЖНО: Используем ссылку, чтобы менять состояние (current_value) внутри карты
    for (auto& [usage, def] : it->second) {
        int32_t value = extractValue(data, dataLen, def, usage);
        
        bool pressed = (value != 0);
        applyUsageToState(usage, def, pressed);
    }
}

int32_t UARTReportProcessor::extractValue(const uint8_t* report, size_t len, usage_def_t& def, uint32_t target_usage) { // Note: usage_def_t& def is not const anymore if we update state
    // Проверяем границы с учетом размера элемента
    if (!report || (def.bitpos / 8) >= len) return 0;

    int32_t value = 0;

    if (def.is_array) {
        uint32_t byte_offset = def.bitpos / 8;
        // Для клавиатур обычно 8-битные коды клавиш
        uint8_t search_value = target_usage & 0xFF; 
        
        // Доп. защита границ
        if (byte_offset + def.count > len) return 0;

        for (uint32_t i = 0; i < def.count; i++) {
            if (report[byte_offset + i] == search_value) {
                return 1;
            }
        }
        return 0;
    }

    // Variable parsing
    uint32_t bitOffset = def.bitpos;
    
    for (uint8_t i = 0; i < def.size; ++i) {
        uint32_t totalBit = bitOffset + i;
        uint32_t byteIdx = totalBit / 8;
        uint32_t bitIdx  = totalBit % 8;
        
        if (byteIdx >= len) break;

        if (report[byteIdx] & (1 << bitIdx)) {
            value |= (1 << i);
        }
    }

    // Scaling
    if (def.should_be_scaled) {
        if (value > def.logical_maximum) value = def.logical_maximum;
        if (value < def.logical_minimum) value = def.logical_minimum;
    }

    // Relative value handling (без указателей)
    if (def.is_relative) {
        // Если это первый замер, просто инициализируем, иначе суммируем
        if (!def.has_value) {
            def.current_value = value;
            def.has_value = true;
        } else {
            def.current_value += value;
        }
        // Для relative обычно интересен сам value (дельта), но если вам нужен абсолют:
        // return def.current_value; 
        // Если вы обрабатываете дельты (например мышь), возвращаем value как есть:
        return static_cast<int32_t>(value);
    }

    return static_cast<int32_t>(value);
}

void UARTReportProcessor::updateUsageState(usage_def_t& usage, int32_t value) {
    // Этот метод теперь можно удалить или упростить, так как состояние в структуре
    usage.current_value = value;
    usage.has_value = true;
}

void UARTReportProcessor::applyUsageToState(uint32_t usage, const usage_def_t& def, bool pressed) {
    uint32_t mask = 0;

    switch (usage) {
        case 0x07002C: // Keyboard Space
            mask = GAMEPAD_MASK_B1;
            break;
        case 0x090001: // Mouse Left
            mask = GAMEPAD_MASK_B2;
            break;
        // Добавьте остальные кнопки здесь
        default:
            return;
    }

    if (pressed) {
        uartState.buttons |= mask;
    } else {
        uartState.buttons &= ~mask;
    }
}
