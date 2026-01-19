#include "peripheral_uart.h"

PeripheralUART::PeripheralUART() {
    _UART = uart0;
    _TX = -1;
    _RX = -1;
    _CTS = -1;
    _RTS = -1;
    _Speed = UART_DEFAULT_BAUDRATE;
    configured = false;
}

void PeripheralUART::setConfig(uint8_t block, int8_t tx, int8_t rx, int8_t cts, int8_t rts, uint32_t speed) {
    if (block < NUM_UARTS) {
        _UART = _hardwareBlocks[block];
        _TX = tx;
        _RX = rx;
        _CTS = cts;
        _RTS = rts;
        _Speed = speed;
        setup();
    }
}

void PeripheralUART::setup() {
    configured = false;
    if (_TX == -1 || _RX == -1) return;

    // Инициализация
    uart_init(_UART, _Speed);
    gpio_set_function(_TX, GPIO_FUNC_UART);
    gpio_set_function(_RX, GPIO_FUNC_UART);

    bool useFlowControl = (_CTS != -1 && _RTS != -1);
    if (useFlowControl) {
        gpio_set_function(_CTS, GPIO_FUNC_UART);
        gpio_set_function(_RTS, GPIO_FUNC_UART);
        uart_set_hw_flow(_UART, true, true);
    } else {
        uart_set_hw_flow(_UART, false, false);
    }

    uart_set_fifo_enabled(_UART, true);
    uart_set_translate_crlf(_UART, false);

    // Только теперь считаем, что все настроено
    configured = true;
}

bool PeripheralUART::isWritable() {
    return configured && uart_is_writable(_UART);
}

bool PeripheralUART::isReadable() {
    return configured && uart_is_readable(_UART);
}

void PeripheralUART::write(uint8_t data) {
    if (configured) {
        uart_putc_raw(_UART, data);
    }
}

uint8_t PeripheralUART::read() {
    if (configured) {
        return uart_getc(_UART);
    }
    return 0;
}