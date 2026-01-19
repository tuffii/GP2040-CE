#ifndef _DUAL_PICO_HOST_H_
#define _DUAL_PICO_HOST_H_

#include "gpaddon.h"
#include "peripheralmanager.h"
#include "dual_protocol.h"

#define DualPicoHostName "DualPicoHost"

class DualPicoHostAddon : public GPAddon {
public:
    virtual bool available();
    virtual void setup();
    virtual void process();
    virtual void preprocess() {}
    virtual void postprocess(bool sent) {}
    virtual void reinit();
    virtual std::string name() { return DualPicoHostName; }

private:
    // Работа с UART
    void process_serial();
    void serial_write(const uint8_t* data, uint16_t len);
    void serial_putc_escaped(uint8_t b, uart_inst_t* uart);
    
    // Обработка пакетов
    void handle_packet(const uint8_t* data, uint16_t len);
    
    // Переменные для чтения буфера
    uint8_t rx_buffer[512]; // Максимальный размер пакета
    uint16_t rx_idx;
    bool rx_escaped;
    
    // Ссылка на активный UART драйвер
    uart_inst_t* active_uart;
};

#endif