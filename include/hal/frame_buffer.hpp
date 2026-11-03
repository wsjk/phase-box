#pragma once
// RAM frame buffer in SSD1306 page layout. The app draws into this (no I2C involved); a separate
// core copies finished frames to the real display, so the main loop never waits on the bus.
#include "hal/pico_hal_display.hpp"   // DisplayInterface + font5x7
#include <cstdint>
#include <cstring>

namespace phasebox::hal {

class FrameBuffer : public DisplayInterface {
public:
    static constexpr size_t MAX_BYTES = 128 * 64 / 8;

    explicit FrameBuffer(uint16_t width = 128, uint16_t height = 32)
        : width_(width > 128 ? 128 : width), height_(height > 64 ? 64 : height) {
        clear();
    }

    void clear() override { std::memset(buf_, 0, size()); }

    void draw_pixel(int16_t x, int16_t y, bool on) override {
        if (x < 0 || x >= width_ || y < 0 || y >= height_) return;
        const size_t index = static_cast<size_t>(x) + static_cast<size_t>(y / 8) * width_;
        const uint8_t bit = static_cast<uint8_t>(1u << (y % 8));
        if (on) buf_[index] |= bit; else buf_[index] &= static_cast<uint8_t>(~bit);
    }

    void draw_string(int16_t x, int16_t y, const char* str) override {
        if (str == nullptr) return;
        int16_t cur_x = x;
        while (*str) {
            const char c = *str++;
            if (c >= 32 && c <= 126) {
                const uint8_t* glyph = font5x7::data[c - 32];
                for (int col = 0; col < 5; ++col)
                    for (int row = 0; row < 8; ++row)
                        if (glyph[col] & (1 << row)) draw_pixel(cur_x + col, y + row, true);
            }
            cur_x += 6;   // 5 px glyph + 1 px gap
            if (cur_x >= width_) break;
        }
    }

    void update() override {}   // nothing to send: a separate core pushes data() to the panel

    uint16_t get_width() const override { return width_; }
    uint16_t get_height() const override { return height_; }
    const uint8_t* data() const { return buf_; }
    size_t size() const { return static_cast<size_t>(width_) * (height_ / 8); }

private:
    uint16_t width_, height_;
    uint8_t buf_[MAX_BYTES];
};

} // namespace phasebox::hal
