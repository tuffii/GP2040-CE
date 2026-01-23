#include "uart_report_processor.h"
#include <cstring>
#include <algorithm>

// Обрабатываем новый HID-отчёт
void UARTReportProcessor::processReport(UARTDeviceContext& device, const uint8_t* report, size_t len) {
    if (!report || len == 0 || !device.active) return;

    // Проходим по всем report_id и usage
    for (auto& [report_id, usageMap] : device.usages) {
        for (auto& [usage, usageDef] : usageMap) {
            int32_t value = extractValue(report, len, usageDef);
            updateUsageState(usageDef, value);

            // ⚠️ Генерация события для нового значения
            generateEvent(device, usageDef);
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
void UARTReportProcessor::generateEvent(UARTDeviceContext& device, const usage_def_t& usage) {
    // Здесь можно:
    // - сравнивать *(usage.input_state_0) и *(usage.input_state_n)
    // - формировать событие (кнопка нажата/отпущена, движение оси)
    // - отправлять в другой класс/в систему событий
    // Например:
    // if (*(usage.input_state_0) != *(usage.input_state_n)) { ... }
}
