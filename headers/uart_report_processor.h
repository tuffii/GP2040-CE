#ifndef _UART_REPORT_PROCESSOR_H_
#define _UART_REPORT_PROCESSOR_H_

#include <cstddef>
#include <cstdint>
#include "uart_device_context.h"
#include "uart_input_state.h"

class UARTReportProcessor {
public:

    explicit UARTReportProcessor(UARTInputState& state);

    void processReport(UARTDeviceContext& device, const uint8_t* report, size_t len);

private:
    int32_t extractValue(const uint8_t* report, size_t len, usage_def_t& usage, uint32_t target_usage);

    void updateUsageState(usage_def_t& usage, int32_t value);

    void applyUsageToState(uint32_t usage, const usage_def_t& def, bool pressed);

    UARTInputState& uartState;
};

#endif
