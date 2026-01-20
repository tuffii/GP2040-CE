#ifndef _DUAL_PICO_HOST_H_
#define _DUAL_PICO_HOST_H_

#include "gpaddon.h"
#include "peripheralmanager.h"
#include "dual_protocol.h"

#define DualPicoHostName "DualPicoHost"

// Структура для хранения промежуточного состояния ввода
struct DualHostState {
    uint8_t dpad;
    uint16_t buttons;
    uint16_t lx, ly, rx, ry;
};

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
    
    // Новые методы для обработки ввода
    void process_kbd_report(const uint8_t* report_data);
    void reset_host_state();

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
    
    // Карта типов устройств
    uint8_t dev_type_map[32];

    // Текущее состояние ввода (вместо test_button_pressed)
    DualHostState _host_state;
};

#endif