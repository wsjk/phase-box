#ifndef PHASEBOX_HAL_DISPLAY_HPP
#define PHASEBOX_HAL_DISPLAY_HPP

#include <cstdint>
#include <array>
#include <cstring>
#include "hal/pinout.hpp"

namespace phasebox::hal {

/// Drawing interface implemented by MockHalDisplay (tests) and PicoHalDisplay (SSD1306 over I2C).
class DisplayInterface {
public:
    virtual ~DisplayInterface() = default;
    virtual void clear() = 0;
    virtual void draw_pixel(int16_t x, int16_t y, bool on) = 0;
    virtual void draw_string(int16_t x, int16_t y, const char* str) = 0;
    virtual void update() = 0;
    virtual uint16_t get_width() const = 0;
    virtual uint16_t get_height() const = 0;
};

class HalDisplay {
public:
    static constexpr uint16_t WIDTH = 128;
    static constexpr uint16_t HEIGHT = 32;
    static constexpr uint16_t BUFFER_SIZE = (WIDTH * HEIGHT) / 8; // 512 bytes

    HalDisplay() = default;

    /**
     * @brief Initialize I2C hardware (default SDA GP4, SCL GP5; see hal/pinout.hpp).
     */
    void init(uint8_t sda_pin = pins::OLED_SDA, uint8_t scl_pin = pins::OLED_SCL);

    /**
     * @brief Clear framebuffer (set all pixels to black).
     */
    void clear();

    /**
     * @brief Draw a single pixel at (x, y).
     */
    void draw_pixel(int16_t x, int16_t y, bool color = true);

    /**
     * @brief Draw an ASCII text string using built-in 5x7 font.
     */
    void draw_string(int16_t x, int16_t y, const char* str, bool color = true);
    void draw_char(int16_t x, int16_t y, char c, bool color = true);

    /**
     * @brief Flush 512-byte framebuffer over I2C to SSD1306 display controller.
     */
    void render();

#ifdef HOST_BUILD
    const std::array<uint8_t, BUFFER_SIZE>& get_buffer() const { return buffer_; }
    uint32_t get_render_count() const { return render_count_; }
#endif

private:
    std::array<uint8_t, BUFFER_SIZE> buffer_{};
    uint8_t sda_pin_{pins::OLED_SDA};
    uint8_t scl_pin_{pins::OLED_SCL};

    uint32_t render_count_{0};
};

} // namespace phasebox::hal

#endif // PHASEBOX_HAL_DISPLAY_HPP
