#include <gtest/gtest.h>
#include "hal/hal_display.hpp"

using namespace phasebox::hal;

TEST(HalDisplayTest, InitialBufferClear) {
    HalDisplay display;
    display.init(6, 7);

    const auto& buf = display.get_buffer();
    for (uint8_t byte : buf) {
        EXPECT_EQ(byte, 0x00);
    }
}

TEST(HalDisplayTest, DrawPixelBounds) {
    HalDisplay display;
    display.init(6, 7);

    // Set pixel (0, 0) -> sets bit 0 of byte 0 in framebuffer to 1 (0x01)
    display.draw_pixel(0, 0, true);
    EXPECT_EQ(display.get_buffer()[0], 0x01);

    // Out-of-bounds draws should be safely ignored
    display.draw_pixel(-1, 0, true);
    display.draw_pixel(128, 0, true);
    display.draw_pixel(0, 32, true);
    EXPECT_EQ(display.get_buffer()[0], 0x01);
}

TEST(HalDisplayTest, RenderCountIncrement) {
    HalDisplay display;
    display.init(6, 7);

    EXPECT_EQ(display.get_render_count(), 0u);
    display.render();
    EXPECT_EQ(display.get_render_count(), 1u);
    display.render();
    EXPECT_EQ(display.get_render_count(), 2u);
}
