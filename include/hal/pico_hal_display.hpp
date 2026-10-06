#pragma once
#include "hal/hal_display.hpp"
#include <cstdint>
#include <cstring>
#include <algorithm>

#if __has_include("hardware/i2c.h") && __has_include("hardware/gpio.h")
#include "hardware/i2c.h"
#include "hardware/gpio.h"
#define PHASEBOX_PICO_I2C_AVAILABLE 1
#endif

namespace phasebox::hal {

// Basic 5x7 ASCII font table for fast on-chip rendering (characters 32 ' ' to 126 '~')
namespace font5x7 {
    static const uint8_t data[][5] = {
        {0x00, 0x00, 0x00, 0x00, 0x00}, // 32 ' '
        {0x00, 0x00, 0x5F, 0x00, 0x00}, // 33 '!'
        {0x00, 0x07, 0x00, 0x07, 0x00}, // 34 '"'
        {0x14, 0x7F, 0x14, 0x7F, 0x14}, // 35 '#'
        {0x24, 0x2A, 0x7F, 0x2A, 0x12}, // 36 '$'
        {0x23, 0x13, 0x08, 0x64, 0x62}, // 37 '%'
        {0x36, 0x49, 0x55, 0x22, 0x50}, // 38 '&'
        {0x00, 0x05, 0x03, 0x00, 0x00}, // 39 '''
        {0x00, 0x1C, 0x22, 0x41, 0x00}, // 40 '('
        {0x00, 0x41, 0x22, 0x1C, 0x00}, // 41 ')'
        {0x14, 0x08, 0x3E, 0x08, 0x14}, // 42 '*'
        {0x08, 0x08, 0x3E, 0x08, 0x08}, // 43 '+'
        {0x00, 0x50, 0x30, 0x00, 0x00}, // 44 ','
        {0x08, 0x08, 0x08, 0x08, 0x08}, // 45 '-'
        {0x00, 0x60, 0x60, 0x00, 0x00}, // 46 '.'
        {0x20, 0x10, 0x08, 0x04, 0x02}, // 47 '/'
        {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 48 '0'
        {0x00, 0x42, 0x7F, 0x40, 0x00}, // 49 '1'
        {0x42, 0x61, 0x51, 0x49, 0x46}, // 50 '2'
        {0x21, 0x41, 0x45, 0x4B, 0x31}, // 51 '3'
        {0x18, 0x14, 0x12, 0x7F, 0x10}, // 52 '4'
        {0x27, 0x45, 0x45, 0x45, 0x39}, // 53 '5'
        {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 54 '6'
        {0x01, 0x71, 0x09, 0x05, 0x03}, // 55 '7'
        {0x36, 0x49, 0x49, 0x49, 0x36}, // 56 '8'
        {0x06, 0x49, 0x49, 0x29, 0x1E}, // 57 '9'
        {0x00, 0x36, 0x36, 0x00, 0x00}, // 58 ':'
        {0x00, 0x56, 0x36, 0x00, 0x00}, // 59 ';'
        {0x08, 0x14, 0x22, 0x41, 0x00}, // 60 '<'
        {0x14, 0x14, 0x14, 0x14, 0x14}, // 61 '='
        {0x00, 0x41, 0x22, 0x14, 0x08}, // 62 '>'
        {0x02, 0x01, 0x51, 0x09, 0x06}, // 63 '?'
        {0x32, 0x49, 0x79, 0x41, 0x3E}, // 64 '@'
        {0x7E, 0x11, 0x11, 0x11, 0x7E}, // 65 'A'
        {0x7F, 0x49, 0x49, 0x49, 0x36}, // 66 'B'
        {0x3E, 0x41, 0x41, 0x41, 0x22}, // 67 'C'
        {0x7F, 0x41, 0x41, 0x22, 0x1C}, // 68 'D'
        {0x7F, 0x49, 0x49, 0x49, 0x41}, // 69 'E'
        {0x7F, 0x09, 0x09, 0x09, 0x01}, // 70 'F'
        {0x3E, 0x41, 0x49, 0x49, 0x7A}, // 71 'G'
        {0x7F, 0x08, 0x08, 0x08, 0x7F}, // 72 'H'
        {0x00, 0x41, 0x7F, 0x41, 0x00}, // 73 'I'
        {0x20, 0x40, 0x41, 0x3F, 0x01}, // 74 'J'
        {0x7F, 0x08, 0x14, 0x22, 0x41}, // 75 'K'
        {0x7F, 0x40, 0x40, 0x40, 0x40}, // 76 'L'
        {0x7F, 0x02, 0x0C, 0x02, 0x7F}, // 77 'M'
        {0x7F, 0x04, 0x08, 0x10, 0x7F}, // 78 'N'
        {0x3E, 0x41, 0x41, 0x41, 0x3E}, // 79 'O'
        {0x7F, 0x09, 0x09, 0x09, 0x06}, // 80 'P'
        {0x3E, 0x41, 0x51, 0x21, 0x5E}, // 81 'Q'
        {0x7F, 0x09, 0x19, 0x29, 0x46}, // 82 'R'
        {0x46, 0x49, 0x49, 0x49, 0x31}, // 83 'S'
        {0x01, 0x01, 0x7F, 0x01, 0x01}, // 84 'T'
        {0x3F, 0x40, 0x40, 0x40, 0x3F}, // 85 'U'
        {0x1F, 0x20, 0x40, 0x20, 0x1F}, // 86 'V'
        {0x3F, 0x40, 0x38, 0x40, 0x3F}, // 87 'W'
        {0x63, 0x14, 0x08, 0x14, 0x63}, // 88 'X'
        {0x07, 0x08, 0x70, 0x08, 0x07}, // 89 'Y'
        {0x61, 0x51, 0x49, 0x45, 0x43}, // 90 'Z'
        {0x00, 0x7F, 0x41, 0x41, 0x00}, // 91 '['
        {0x02, 0x04, 0x08, 0x10, 0x20}, // 92 '\'
        {0x00, 0x41, 0x41, 0x7F, 0x00}, // 93 ']'
        {0x04, 0x02, 0x01, 0x02, 0x04}, // 94 '^'
        {0x40, 0x40, 0x40, 0x40, 0x40}, // 95 '_'
        {0x00, 0x01, 0x02, 0x04, 0x00}, // 96 '`'
        {0x20, 0x54, 0x54, 0x54, 0x78}, // 97 'a'
        {0x7F, 0x48, 0x44, 0x44, 0x38}, // 98 'b'
        {0x38, 0x44, 0x44, 0x44, 0x20}, // 99 'c'
        {0x38, 0x44, 0x44, 0x48, 0x7F}, // 100 'd'
        {0x38, 0x54, 0x54, 0x54, 0x18}, // 101 'e'
        {0x08, 0x7E, 0x09, 0x01, 0x02}, // 102 'f'
        {0x0C, 0x52, 0x52, 0x52, 0x3E}, // 103 'g'
        {0x7F, 0x08, 0x04, 0x04, 0x78}, // 104 'h'
        {0x00, 0x44, 0x7D, 0x40, 0x00}, // 105 'i'
        {0x20, 0x40, 0x44, 0x3D, 0x00}, // 106 'j'
        {0x7F, 0x10, 0x28, 0x44, 0x00}, // 107 'k'
        {0x00, 0x41, 0x7F, 0x40, 0x00}, // 108 'l'
        {0x7C, 0x04, 0x18, 0x04, 0x78}, // 109 'm'
        {0x7C, 0x08, 0x04, 0x04, 0x78}, // 110 'n'
        {0x38, 0x44, 0x44, 0x44, 0x38}, // 111 'o'
        {0x7C, 0x14, 0x14, 0x14, 0x08}, // 112 'p'
        {0x08, 0x14, 0x14, 0x18, 0x7C}, // 113 'q'
        {0x7C, 0x08, 0x04, 0x04, 0x08}, // 114 'r'
        {0x48, 0x54, 0x54, 0x54, 0x20}, // 115 's'
        {0x04, 0x3F, 0x44, 0x40, 0x20}, // 116 't'
        {0x3C, 0x40, 0x40, 0x20, 0x7C}, // 117 'u'
        {0x1C, 0x20, 0x40, 0x20, 0x1C}, // 118 'v'
        {0x3C, 0x40, 0x30, 0x40, 0x3C}, // 119 'w'
        {0x44, 0x28, 0x10, 0x28, 0x44}, // 120 'x'
        {0x0C, 0x50, 0x50, 0x50, 0x3C}, // 121 'y'
        {0x44, 0x64, 0x54, 0x4C, 0x44}, // 122 'z'
        {0x00, 0x08, 0x36, 0x41, 0x00}, // 123 '{'
        {0x00, 0x00, 0x7F, 0x00, 0x00}, // 124 '|'
        {0x00, 0x41, 0x36, 0x08, 0x00}, // 125 '}'
        {0x08, 0x08, 0x2A, 0x1C, 0x08}  // 126 '~'
    };
}

class PicoHalDisplay : public HalDisplay {
public:
    static constexpr uint8_t DEFAULT_I2C_ADDR = 0x3C;
    static constexpr uint32_t I2C_SPEED_FAST_PLUS = 1000000; // 1 MHz Fast-Mode Plus

#if defined(PHASEBOX_PICO_I2C_AVAILABLE)
    PicoHalDisplay(i2c_inst_t* i2c = i2c0, uint sda_pin = 4, uint scl_pin = 5,
                   uint8_t addr = DEFAULT_I2C_ADDR, uint16_t width = 128, uint16_t height = 32)
        : i2c_(i2c), sda_pin_(sda_pin), scl_pin_(scl_pin), addr_(addr), width_(width), height_(height) {
        buffer_size_ = width_ * (height_ / 8);
        buffer_ = new uint8_t[buffer_size_];
        clear();
    }

    ~PicoHalDisplay() override {
        delete[] buffer_;
    }

    void init() {
        i2c_init(i2c_, I2C_SPEED_FAST_PLUS);
        gpio_set_function(sda_pin_, GPIO_FUNC_I2C);
        gpio_set_function(scl_pin_, GPIO_FUNC_I2C);
        gpio_pull_up(sda_pin_);
        gpio_pull_up(scl_pin_);

        // SSD1306 Initialization sequence for 128x32
        send_cmd(0xAE); // Display OFF
        send_cmd(0xD5); // Set Display Clock Divide Ratio
        send_cmd(0x80);
        send_cmd(0xA8); // Set Multiplex Ratio
        send_cmd(height_ - 1);
        send_cmd(0xD3); // Set Display Offset
        send_cmd(0x00);
        send_cmd(0x40); // Set Display Start Line
        send_cmd(0x8D); // Enable Charge Pump
        send_cmd(0x14);
        send_cmd(0x20); // Memory Addressing Mode: Horizontal
        send_cmd(0x00);
        send_cmd(0xA1); // Segment Re-map: col 127 mapped to SEG0
        send_cmd(0xC8); // COM Output Scan Direction: remapped
        send_cmd(0xDA); // Set COM Pins HW Configuration
        send_cmd(height_ == 64 ? 0x12 : 0x02);
        send_cmd(0x81); // Set Contrast Control
        send_cmd(0x7F);
        send_cmd(0xD9); // Set Pre-charge Period
        send_cmd(0xF1);
        send_cmd(0xDB); // Set VCOMH Deselect Level
        send_cmd(0x40);
        send_cmd(0xA4); // Output Follows RAM
        send_cmd(0xA6); // Normal Display (1 = on, 0 = off)
        send_cmd(0xAF); // Display ON

        clear();
        update();
    }

    void clear() override {
        std::memset(buffer_, 0, buffer_size_);
    }

    void draw_pixel(int16_t x, int16_t y, bool on) override {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
        size_t index = x + (y / 8) * width_;
        uint8_t bit = 1 << (y % 8);
        if (on) {
            buffer_[index] |= bit;
        } else {
            buffer_[index] &= ~bit;
        }
    }

    void draw_string(int16_t x, int16_t y, const char* str) override {
        if (str == nullptr) return;
        int16_t cur_x = x;
        while (*str) {
            char c = *str++;
            if (c >= 32 && c <= 126) {
                const uint8_t* char_data = font5x7::data[c - 32];
                for (int col = 0; col < 5; ++col) {
                    uint8_t line = char_data[col];
                    for (int row = 0; row < 8; ++row) {
                        if (line & (1 << row)) {
                            draw_pixel(cur_x + col, y + row, true);
                        }
                    }
                }
            }
            cur_x += 6; // 5 pixels width + 1 pixel spacing
            if (cur_x + 6 > width_) break;
        }
    }

    void update() override {
        // Set column and page address to fill the screen
        send_cmd(0x21); // Set Column Address
        send_cmd(0);
        send_cmd(width_ - 1);

        send_cmd(0x22); // Set Page Address
        send_cmd(0);
        send_cmd((height_ / 8) - 1);

        // Write buffer in chunks (0x40 = data byte prefix)
        uint8_t chunk[33];
        chunk[0] = 0x40; // Co = 0, D/C# = 1 (Data)

        for (size_t i = 0; i < buffer_size_; i += 32) {
            size_t len = std::min(static_cast<size_t>(32), buffer_size_ - i);
            std::memcpy(&chunk[1], &buffer_[i], len);
            i2c_write_blocking(i2c_, addr_, chunk, len + 1, false);
        }
    }

    uint16_t get_width() const override { return width_; }
    uint16_t get_height() const override { return height_; }

private:
    void send_cmd(uint8_t cmd) {
        uint8_t buf[2] = {0x00, cmd}; // Co = 0, D/C# = 0 (Command)
        i2c_write_blocking(i2c_, addr_, buf, 2, false);
    }

    i2c_inst_t* i2c_;
    uint sda_pin_;
    uint scl_pin_;
    uint8_t addr_;
    uint16_t width_;
    uint16_t height_;
    size_t buffer_size_;
    uint8_t* buffer_{nullptr};

#else
    // Fallback stub for host simulation build
    PicoHalDisplay(uint16_t = 128, uint16_t = 32) {}
    void init() {}
    void clear() override {}
    void draw_pixel(int16_t, int16_t, bool) override {}
    void draw_string(int16_t, int16_t, const char*) override {}
    void update() override {}
    uint16_t get_width() const override { return 128; }
    uint16_t get_height() const override { return 32; }
#endif
};

} // namespace phasebox::hal
