#ifndef _UART_DESCRIPTOR_PARSER_H_
#define _UART_DESCRIPTOR_PARSER_H_

#include <cstdint>
#include <unordered_map>
#include <deque>
#include "uart_device_context.h"

constexpr uint8_t HID_INPUT = 0x80;
constexpr uint8_t HID_COLLECTION = 0xA0;
constexpr uint8_t HID_USAGE_PAGE = 0x04;
constexpr uint8_t HID_REPORT_SIZE = 0x74;
constexpr uint8_t HID_REPORT_ID = 0x84;
constexpr uint8_t HID_REPORT_COUNT = 0x94;
constexpr uint8_t HID_USAGE = 0x08;
constexpr uint8_t HID_USAGE_MINIMUM = 0x18;
constexpr uint8_t HID_USAGE_MAXIMUM = 0x28;
constexpr uint8_t HID_LOGICAL_MINIMUM = 0x14;
constexpr uint8_t HID_LOGICAL_MAXIMUM = 0x24;

class UARTDescriptorParser {
public:
    UARTDescriptorParser() = default;

    void parseDeviceDescriptor(UARTDeviceContext& device, const uint8_t* reportDescriptor, uint16_t len);

private:
    void markUsage(
        UsageMap& usageMap,
        uint32_t usage,
        uint8_t report_id,
        uint16_t bitpos,
        uint8_t size,
        bool is_relative,
        int32_t logical_min,
        int32_t logical_max,
        bool is_array = false,
        uint32_t index = 0,
        uint32_t count = 0,
        uint32_t usage_max = 0);

    void processItem(UARTDeviceContext& device,
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
                     int32_t& unsigned_logical_max);
};

// void applyQuirks(UARTDeviceContext& device,
//                  const uint8_t* reportDescriptor,
//                  uint16_t len);
// void addSyntheticDpadUsages(UsageMap& usageMap);

#endif
