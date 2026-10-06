#pragma once
#include <cstdint>

namespace phasebox::hal {

class HalDisplay {
public:
    virtual ~HalDisplay() = default;
    virtual void clear() = 0;
    virtual void draw_pixel(int16_t x, int16_t y, bool on) = 0;
    virtual void draw_string(int16_t x, int16_t y, const char* str) = 0;
    virtual void update() = 0;
    virtual uint16_t get_width() const = 0;
    virtual uint16_t get_height() const = 0;
};

} // namespace phasebox::hal
