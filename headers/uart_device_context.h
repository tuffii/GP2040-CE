#ifndef _UART_DEVICE_CONTEXT_H_
#define _UART_DEVICE_CONTEXT_H_

#include <cstdint>
#include <unordered_map>
#include "types.h"

// report_id -> usage -> usage_def
using UsageMap = std::unordered_map<uint8_t, std::unordered_map<uint32_t, usage_def_t>>;

struct UARTDeviceContext {
    uint8_t dev_addr;
    uint8_t interface;
    uint16_t vid;
    uint16_t pid;

    UsageMap usages;

    bool hasReportId;

    bool active;

    UARTDeviceContext()
        : dev_addr(0),
          interface(0),
          vid(0),
          pid(0),
          hasReportId(false),
          active(false) {}
};

#endif
