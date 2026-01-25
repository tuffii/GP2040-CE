#include "uart_protocol.h"
#include "uart_descriptor_parser.h"
#include <cstring>
#include <algorithm>
#include "pico/stdlib.h"

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

void UARTDescriptorParser::parseDeviceDescriptor(UARTDeviceContext& device,
                                             const uint8_t* reportDescriptor,
                                             uint16_t len) {
    device.usages.clear();
    device.hasReportId = false;

     uint8_t current_report_id = 0;
    uint32_t report_size = 0;
    uint32_t report_count = 0;
    uint32_t usage_page = 0;

    std::deque<uint32_t> usages;
    uint32_t usage_min = 0;
    uint32_t usage_max = 0;

    int32_t logical_min = 0;
    int32_t logical_max = 0;

    std::map<uint8_t, uint32_t> bit_counters;

    uint16_t idx = 0;
    while (idx < len) {
        uint8_t key = reportDescriptor[idx++];
        uint8_t item = key & 0xFC;
        uint8_t item_size = key & 0x03;

        if (item_size == 3) item_size = 4;

        uint32_t value = 0;
        for (int i = 0; i < item_size; i++) {
            if (idx < len) {
                value |= reportDescriptor[idx++] << (i * 8);
            }
        }

        processItem(device, item, value, item_size,
                    current_report_id, report_size, report_count,
                    usage_page, usages, usage_min, usage_max,
                    logical_min, logical_max,
                    bit_counters);
    }

    // applyQuirks(device, reportDescriptor, len);
    // addSyntheticDpadUsages(device.usages);
}

static int32_t signExtend(uint32_t value, uint8_t size_bytes) {
    uint8_t bits = size_bytes * 8;
    int32_t sign_bit = 1 << (bits - 1);

    if (value & sign_bit) {
        return (int32_t)(value | (~((1 << bits) - 1)));
    }
    return (int32_t)value;
}

void UARTDescriptorParser::processItem(UARTDeviceContext& device,
                                       uint8_t item,
                                       uint32_t value,
                                       uint8_t item_size,
                                       uint8_t& current_report_id,
                                       uint32_t& report_size,
                                       uint32_t& report_count,
                                       uint32_t& usage_page,
                                       std::deque<uint32_t>& usages,
                                       uint32_t& usage_min,
                                       uint32_t& usage_max,
                                       int32_t& logical_min,
                                       int32_t& logical_max,
                                       std::map<uint8_t, uint32_t>& bit_counters) {
    
    switch (item) {
        case HID_INPUT: {
            bool is_const    = (value & 0x01); // Constant (padding)
            bool is_variable = (value & 0x02); // Variable (bitmap)
            bool is_relative = (value & 0x04); // Relative (mouse delta)
            // Если не Constant и не Variable, значит Array

            uint32_t total_bits = report_size * report_count;
            uint32_t current_bit = bit_counters[current_report_id];

            if (is_const) {
                // Это заполнитель, просто двигаем счетчик
                bit_counters[current_report_id] += total_bits;
            } 
            else if (is_variable) {
                // Обработка Variable (каждый бит - отдельное значение)
                // Пример: Кнопки мыши или модификаторы клавиатуры
                
                // Если задан диапазон (Usage Min/Max)
                if (usage_min != 0 || usage_max != 0) {
                    // Генерируем usages из диапазона, если список пуст
                    if (usages.empty()) {
                         for (uint32_t u = usage_min; u <= usage_max; u++) {
                             usages.push_back(u);
                         }
                    }
                }

                // Распределяем биты по usages
                for (uint32_t i = 0; i < report_count; i++) {
                    uint32_t u = 0;
                    if (i < usages.size()) {
                        u = usages[i];
                    } else if (usages.size() > 0) {
                        // Если usages меньше чем count, последний повторяется или игнорируется
                        // (спецификация HID сложная, берем последний валидный или 0)
                        u = usages.back(); 
                    }

                    if (u != 0) {
                        markUsage(device.usages, u, current_report_id, current_bit, report_size, 
                                  is_relative, logical_min, logical_max);
                    }
                    current_bit += report_size;
                }
                bit_counters[current_report_id] = current_bit;
            } 
            else { 
                // Обработка Array (Массив значений)
                // Пример: Клавиши клавиатуры (Key array)
                // В этом режиме report_count элементов (обычно байтов) представляют индексы нажатых клавиш.
                
                // Для маппинга нам нужно создать запись для КАЖДОЙ возможной клавиши из диапазона Min-Max,
                // чтобы processReport мог найти их. Все они указывают на ОДИН И ТОТ ЖЕ массив данных.

                uint32_t range_min = usage_min;
                uint32_t range_max = usage_max;

                // Если min/max не заданы, пробуем взять из списка usages (редкий кейс для Array, но возможный)
                if (range_min == 0 && range_max == 0 && !usages.empty()) {
                    range_min = usages.front();
                    range_max = usages.back(); // Упрощение
                }

                if (range_max > range_min + 255) {
                    range_max = range_min + 255;
                }
                
                // ВАЖНО: Регистрируем usage для каждого возможного скан-кода
                if (range_max >= range_min) {
                    for (uint32_t u = range_min; u <= range_max; u++) {
                         markUsage(device.usages, u, current_report_id, current_bit, report_size, 
                                   is_relative, logical_min, logical_max, 
                                   true, report_count, range_max); // is_array = true
                    }
                }

                // Сдвигаем счетчик битов на весь массив целиком
                bit_counters[current_report_id] += total_bits;
            }

            // Очистка временных списков после Input
            usages.clear();
            usage_min = 0;
            usage_max = 0;
            break;
        }
        
        case HID_USAGE_PAGE:
            usage_page = value;
            break;

        case HID_REPORT_SIZE:
            report_size = value;
            break;

        case HID_REPORT_ID:
            current_report_id = value;
            device.hasReportId = true;
            break;

        case HID_REPORT_COUNT:
            report_count = value;
            break;

        case HID_USAGE:
            // Если размер item <= 2 байт, добавляем usage_page
            usages.push_back(item_size <= 2 ? (usage_page << 16 | value) : value);
            break;

        case HID_USAGE_MINIMUM:
            usage_min = item_size <= 2 ? (usage_page << 16 | value) : value;
            break;

        case HID_USAGE_MAXIMUM:
            usage_max = item_size <= 2 ? (usage_page << 16 | value) : value;
            break;

        case HID_LOGICAL_MINIMUM:
            logical_min = signExtend(value, item_size);
            break;

        case HID_LOGICAL_MAXIMUM:
            logical_max = signExtend(value, item_size);
            break;
            
        case HID_COLLECTION:
            usages.clear();
            usage_min = 0;
            usage_max = 0;
            break;

        default:
            break;
    }
}

void UARTDescriptorParser::markUsage(
    UsageMap& usageMap,
    uint32_t usage,
    uint8_t report_id,
    uint16_t bitpos,
    uint8_t size,
    bool is_relative,
    int32_t logical_min,
    int32_t logical_max,
    bool is_array,
    uint32_t count,
    uint32_t usage_max
) {

    auto& report_usages = usageMap[report_id];
    auto it = report_usages.find(usage);

    if (it != report_usages.end()) {
        // Если существующее определение - Variable (не массив), 
        // а новое - Array (массив), то игнорируем новое.
        // Это предотвращает перезапись конкретных битов модификаторов 
        // общим диапазоном массива клавиш (0-255).
        if (!it->second.is_array && is_array) {
            return;
        }
    }

    usage_def_t def;
    def.report_id = report_id;
    def.bitpos = bitpos; // ТЕПЕРЬ здесь реальная позиция бита
    def.size = size;
    def.is_relative = is_relative;
    def.is_array = is_array;
    def.index = 0; // Для Variable index рассчитывается динамически в цикле выше
    def.count = count;
    def.usage_maximum = usage_max;
    def.logical_minimum = logical_min;
    def.logical_maximum = logical_max;

    usageMap[report_id][usage] = def;

    if (is_relative && logical_min < 0) {
        // debug_blink(5, 50); // устройство с signed осью
    }
}

// void UARTDescriptorParser::applyQuirks(UARTDeviceContext& device,
//                                    const uint8_t* reportDescriptor,
//                                    uint16_t len) {
//     добавить специальные исправления для конкретных устройств
// }

// void UARTDescriptorParser::addSyntheticDpadUsages(UsageMap& usageMap) {
//     добавить виртуальные D-Pad кнопки на основе существующих usage
// }