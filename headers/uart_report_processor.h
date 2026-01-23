#ifndef _UART_REPORT_PROCESSOR_H_
#define _UART_REPORT_PROCESSOR_H_

#include <cstddef>
#include <cstdint>
#include "uart_device_context.h"

class UARTReportProcessor {
public:
    void processReport(UARTDeviceContext& device, const uint8_t* report, size_t len);

private:
    int32_t extractValue(const uint8_t* report, size_t len, const usage_def_t& usage);

    void updateUsageState(usage_def_t& usage, int32_t value);

    void generateEvent(UARTDeviceContext& device, const usage_def_t& usage);
};

#endif
