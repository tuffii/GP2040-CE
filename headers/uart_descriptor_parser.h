#ifndef _UART_DESCRIPTOR_PARSER_H_
#define _UART_DESCRIPTOR_PARSER_H_

#include <cstdint>
#include <deque>
#include <map>
#include "uart_device_context.h"

#define HID_INPUT           0x80
#define HID_OUTPUT          0x90
#define HID_FEATURE         0xB0
#define HID_COLLECTION      0xA0
#define HID_END_COLLECTION  0xC0
#define HID_USAGE_PAGE      0x04
#define HID_LOGICAL_MINIMUM 0x14
#define HID_LOGICAL_MAXIMUM 0x24
#define HID_USAGE_MINIMUM   0x18
#define HID_USAGE_MAXIMUM   0x28
#define HID_REPORT_SIZE     0x74
#define HID_REPORT_ID       0x84
#define HID_REPORT_COUNT    0x94
#define HID_USAGE           0x08

class UARTDescriptorParser {
public:
    UARTDescriptorParser() = default;

    void parseDeviceDescriptor(UARTDeviceContext& device, const uint8_t* reportDescriptor, uint16_t len);

private:
    void markUsage(UsageMap& usageMap,
                   uint32_t usage,
                   uint8_t report_id,
                   uint16_t bitpos,
                   uint8_t size,
                   bool is_relative,
                   int32_t logical_min,
                   int32_t logical_max,
                   bool is_array = false,
                   uint32_t count = 0,
                   uint32_t usage_max = 0);

    void processItem(UARTDeviceContext& device,
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
                     std::map<uint8_t, uint32_t>& bit_counters);
};

// void applyQuirks(UARTDeviceContext& device,
//                  const uint8_t* reportDescriptor,
//                  uint16_t len);
// void addSyntheticDpadUsages(UsageMap& usageMap);

#endif
