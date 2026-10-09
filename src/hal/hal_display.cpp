#include "hal/hal_display.hpp"

#ifndef HOST_BUILD
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#endif

namespace phasebox {
namespace hal {

void HalDisplay::init(uint8_t sda_pin, uint8_t scl_pin) {
    clear();
}

void HalDisplay::clear() {
    buffer_.fill(0);
    render_count_++;
}

void HalDisplay::render() {
    // Host mock render or I2C transfer
}

void HalDisplay::draw_pixel(int16_t x, int16_t y, bool color) {
    if (x < 0 || x >= 128 || y < 0 || y >= 32) return;
    size_t index = x + (y / 8) * 128;
    if (index < buffer_.size()) {
        if (color) {
            buffer_[index] |= (1 << (y % 8));
        } else {
            buffer_[index] &= ~(1 << (y % 8));
        }
    }
}

} // namespace hal
} // namespace phasebox
