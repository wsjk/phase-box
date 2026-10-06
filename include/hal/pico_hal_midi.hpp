#pragma once
#include "hal/hal_midi.hpp"
#include <cstdint>

#if __has_include("hardware/uart.h") && __has_include("hardware/gpio.h")
#include "hardware/uart.h"
#include "hardware/gpio.h"
#define PHASEBOX_PICO_UART_AVAILABLE 1
#endif

namespace phasebox::hal {

/**
 * @brief RP2040 Hardware UART MIDI Driver
 * 
 * Drives hardware UART at 31250 baud for standard DIN-5 and TRS MIDI output/input.
 */
class PicoHalMidi : public HalMidi {
public:
    static constexpr uint32_t MIDI_BAUD_RATE = 31250;
    static constexpr uint8_t MIDI_STATUS_CC = 0xB0;
    static constexpr uint8_t MIDI_TIMING_CLOCK = 0xF8;

#if defined(PHASEBOX_PICO_UART_AVAILABLE)
    PicoHalMidi(uart_inst_t* uart = uart0, uint tx_pin = 0, uint rx_pin = 1, uint baud_rate = MIDI_BAUD_RATE)
        : uart_(uart), tx_pin_(tx_pin), rx_pin_(rx_pin), baud_rate_(baud_rate) {}

    void init() {
        uart_init(uart_, baud_rate_);
        gpio_set_function(tx_pin_, GPIO_FUNC_UART);
        gpio_set_function(rx_pin_, GPIO_FUNC_UART);
        uart_set_hw_flow(uart_, false, false);
        uart_set_format(uart_, 8, 1, UART_PARITY_NONE);
        uart_set_fifo_enabled(uart_, true);
    }

    void send_cc(uint8_t channel, uint8_t controller, uint8_t value) override {
        uint8_t ch = (channel >= 1 && channel <= 16) ? (channel - 1) : (channel & 0x0F);
        uint8_t status = MIDI_STATUS_CC | (ch & 0x0F);
        uint8_t data1 = controller & 0x7F;
        uint8_t data2 = value & 0x7F;

        uart_putc_raw(uart_, status);
        uart_putc_raw(uart_, data1);
        uart_putc_raw(uart_, data2);
    }

    bool poll_clock_tick() override {
        while (uart_is_readable(uart_)) {
            uint8_t byte = static_cast<uint8_t>(uart_getc(uart_));
            if (byte == MIDI_TIMING_CLOCK) {
                return true;
            }
        }
        return false;
    }

private:
    uart_inst_t* uart_;
    uint tx_pin_;
    uint rx_pin_;
    uint baud_rate_;

#else
    // Fallback stub for host simulation build
    PicoHalMidi(uint32_t = 0, uint32_t = 0, uint32_t = 1, uint32_t = MIDI_BAUD_RATE) {}
    void init() {}
    void send_cc(uint8_t, uint8_t, uint8_t) override {}
    bool poll_clock_tick() override { return false; }
#endif
};

} // namespace phasebox::hal
