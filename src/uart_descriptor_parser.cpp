#include "uart_protocol.h"
#include "uart_descriptor_parser.h"
#include <cstring>
#include <algorithm>

void UARTDescriptorParser::parseDeviceDescriptor(UARTDeviceContext& device,
                                             const uint8_t* reportDescriptor,
                                             uint16_t len) {
    device.usages.clear();
    device.hasReportId = false;

    uint8_t report_id = 0;
    uint32_t report_size = 0;
    uint32_t report_count = 0;
    uint32_t usage_page = 0;
    std::deque<uint32_t> usages;
    uint32_t usage_min = 0;
    uint32_t usage_max = 0;
    int32_t logical_min = 0;
    int32_t logical_max = 0;
    int32_t unsigned_logical_max = 0;

    uint16_t idx = 0;

    while (idx < len) {
        uint8_t item = reportDescriptor[idx] & 0xFC;
        uint8_t item_size = reportDescriptor[idx] & 0x03;
        if (item_size == 3) item_size = 4;
        uint32_t value = 0;
        idx++;
        for (int i = 0; i < item_size; i++) {
            value |= reportDescriptor[idx++] << (i * 8);
        }

        processItem(device, item, value, item_size,
                    report_id, report_size, report_count,
                    usage_page, usages, usage_min, usage_max,
                    logical_min, logical_max, unsigned_logical_max);
    }

    // applyQuirks(device, reportDescriptor, len);
    // addSyntheticDpadUsages(device.usages);
}

void UARTDescriptorParser::processItem(UARTDeviceContext& device,
                                    uint8_t item,
                                    uint32_t value,
                                    uint8_t item_size,
                                    uint8_t& report_id,
                                    uint32_t& report_size,
                                    uint32_t& report_count,
                                    uint32_t& usage_page,
                                    std::deque<uint32_t>& usages,
                                    uint32_t& usage_min,
                                    uint32_t& usage_max,
                                    int32_t& logical_min,
                                    int32_t& logical_max,
                                    int32_t& unsigned_logical_max) {
    switch(item) {
        case HID_INPUT: {
            bool relative = value & (1 << 2);
            if ((value & 0x03) == 0x02) {  // scalar
                if (usage_min && usage_max) {
                    for (uint32_t u = usage_min; u <= usage_max; u++) {
                        markUsage(device.usages, u, report_id, 0, report_size, relative,
                                  logical_min, logical_max);
                    }
                } else {
                    for (auto u : usages) {
                        markUsage(device.usages, u, report_id, 0, report_size, relative,
                                  logical_min, logical_max);
                    }
                    usages.clear();
                }
            }
            break;
        }
        case HID_COLLECTION:
            usages.clear();
            usage_min = 0;
            usage_max = 0;
            break;
        case HID_USAGE_PAGE:
            usage_page = value;
            break;
        case HID_REPORT_SIZE:
            report_size = value;
            break;
        case HID_REPORT_ID:
            report_id = value;
            device.hasReportId = true;
            break;
        case HID_REPORT_COUNT:
            report_count = value;
            break;
        case HID_USAGE:
            usages.push_back(item_size <= 2 ? (usage_page << 16 | value) : value);
            break;
        case HID_USAGE_MINIMUM:
            usage_min = item_size <= 2 ? (usage_page << 16 | value) : value;
            break;
        case HID_USAGE_MAXIMUM:
            usage_max = item_size <= 2 ? (usage_page << 16 | value) : value;
            break;
        case HID_LOGICAL_MINIMUM:
            logical_min = value;
            break;
        case HID_LOGICAL_MAXIMUM:
            logical_max = unsigned_logical_max = value;
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
    uint32_t index,
    uint32_t count,
    uint32_t usage_max
) {
    usage_def_t def;
    def.report_id = report_id;
    def.bitpos = bitpos;
    def.size = size;
    def.is_relative = is_relative;
    def.is_array = is_array;
    def.index = index;
    def.count = count;
    def.usage_maximum = usage_max;
    def.logical_minimum = logical_min;
    def.logical_maximum = logical_max;

    usageMap[report_id][usage] = def;
}

// void UARTDescriptorParser::applyQuirks(UARTDeviceContext& device,
//                                    const uint8_t* reportDescriptor,
//                                    uint16_t len) {
//     добавить специальные исправления для конкретных устройств
// }

// void UARTDescriptorParser::addSyntheticDpadUsages(UsageMap& usageMap) {
//     добавить виртуальные D-Pad кнопки на основе существующих usage
// }