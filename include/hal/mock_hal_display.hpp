#pragma once
#include "hal/hal_display.hpp"
#include <vector>
#include <string>

namespace phasebox::hal {

class MockHalDisplay : public HalDisplay {
public:
    MockHalDisplay(uint16_t width = 128, uint16_t height = 32)
        : width_(width), height_(height), buffer_(width * (height / 8), 0) {}

    void clear() override {
        std::fill(buffer_.begin(), buffer_.end(), 0);
        drawn_strings_.clear();
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
        drawn_strings_.push_back(std::string(str));
    }

    void update() override {
        update_count_++;
    }

    uint16_t get_width() const override { return width_; }
    uint16_t get_height() const override { return height_; }

    uint32_t get_update_count() const { return update_count_; }
    const std::vector<std::string>& get_drawn_strings() const { return drawn_strings_; }
    const std::vector<uint8_t>& get_buffer() const { return buffer_; }

    bool contains_text(const std::string& query) const {
        for (const auto& s : drawn_strings_) {
            if (s.find(query) != std::string::npos) return true;
        }
        return false;
    }

private:
    uint16_t width_;
    uint16_t height_;
    std::vector<uint8_t> buffer_;
    std::vector<std::string> drawn_strings_;
    uint32_t update_count_{0};
};

} // namespace phasebox::hal
