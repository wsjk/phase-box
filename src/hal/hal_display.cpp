#include "hal/hal_display.hpp"

#ifndef HOST_BUILD
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#endif

namespace phasebox::hal {

// Minimal 5x7 ASCII font table subset for numbers and uppercase letters
static const uint8_t FONT5X7[] = {
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
    {0x42, 0x61, 0x51, 0x49, 0x46}, // '2'
    {0x21, 0x41, 0x45, 0x4B, 0x31}, // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x30}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
    {0x00, 0x36, 0x36, 0x00, 0x00}, // ':'
    {0x7F, 0x09, 0x09, 0x09, 0x7F}, // 'A'
    {0x7F, 0x49, 0x49, 0x49, 0x36}, // 'B'
    {0x3E, 0x41, 0x41, 0x41, 0x22}, // 'C'
    {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 'D'
    {0x7F, 0x49, 0x49, 0x49, 0x41}, // 'E'
    {0x7F, 0x09, 0x09, 0x09, 0x01}  // 'F'
};

void HalDisplay::init(uint8_t sda_pin, uint8_t scl_pin) {
    sda_pin_ = sda_pin;
    scl_pin_ = scl_pin;
    clear();

#ifndef HOST_BUILD
    i2c_init(i2c1, 1000000); // 1 MHz Fast-Mode+
    gpio_set_function(sda_pin_, GPIO_FUNC_I2C);
    gpio_set_function(scl_pin_, GPIO_FUNC_I2C);
    gpio_pull_up(sda_pin_);
    gpio_pull_up(scl_pin_);
#endif
}

void HalDisplay::clear() {
    buffer_.fill(0x00);
}

void HalDisplay::draw_pixel(int16_t x, int16_t y, bool color) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;

    uint16_t index = x + (y / 8) * WIDTH;
    uint8_t bit = y % 8;

    if (color) {
        buffer_[index] |= (1 << bit);
    } else {
        buffer_[index] &= ~(1 << bit);
    }
}

void HalDisplay::draw_string(int16_t x, int16_t y, const char* str, bool color) {
    int16_t cursor_x = x;
    while (*str) {
        char c = *str++;
        uint8_t glyph_idx = 0;
        
        if (c >= '0' && c <= '9') glyph_idx = c - '0';
        else if (c == ':') glyph_idx = 10;
        else if (c >= 'A' && c <= 'F') glyph_idx = 11 + (c - 'A');
        else {
            cursor_x += 6;
            continue;
        }

        for (uint8_t col = 0; col < 5; ++col) {
            uint8_t line = FONT5X7[glyph_idx][col];
            for (uint8_t bit = 0; bit < 7; ++bit) {
                if (line & (1 << bit)) {
                    draw_pixel(cursor_x + col, y + bit, color);
                }
            }
        }
        cursor_x += 6; // 5 pixels font width + 1 pixel space
    }
}

void HalDisplay::render() {
#ifndef HOST_BUILD
    // Send 512-byte display buffer over i2c1 to SSD1306 (Address 0x3C)
    uint8_t payload[BUFFER_SIZE + 1];
    payload = 0x40; // Co = 0, D/C# = 1 (Data stream)
    std::memcpy(&payload, buffer_.data(), BUFFER_SIZE);
    i2c_write_blocking(i2c1, 0x3C, payload, sizeof(payload), false);
#else
    render_count_++;
#endif
}

} // namespace phasebox::hal
