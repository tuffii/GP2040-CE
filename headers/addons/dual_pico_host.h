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
    virtual void preprocess();
    virtual void postprocess(bool sent) {}
    virtual void reinit();
    virtual std::string name() { return DualPicoHostName; }

private:
    void process_serial();
    void handle_packet(const uint8_t* data, uint16_t len);
    
    // Отправка
    void send_b_init();
    void serial_write(const uint8_t* data, uint16_t len);
    void serial_putc_escaped(uint8_t b, uart_inst_t* uart);

    uart_inst_t* active_uart;
    
    // Буферы и состояние
    uint8_t rx_buffer[512];
    uint16_t rx_idx;
    bool rx_escaped;

    bool connection_established;
    uint32_t last_handshake_sent;
    
    // Карта типов устройств (0 = нет, 1 = клава, 2 = мышь)
    // Добавлено для отслеживания подключений
    uint8_t dev_type_map[32];

    // Для теста ввода
    bool test_button_pressed;
};

#endif