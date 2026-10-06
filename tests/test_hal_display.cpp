#include "hal/hal_display.hpp"
#include <gtest/gtest.h>

using namespace phasebox::hal;

TEST(HalDisplayTest, InitialBufferClear) {
  HalDisplay display;
  display.init(6, 7);

  const auto &buf = display.get_buffer();
  for (uint8_t byte : buf) {
    EXPECT_EQ(byte, 0x00);
  }
}

TEST(HalDisplayTest, DrawPixelBounds) {
  HalDisplay display;
  display.init(6, 7);

  // Set pixel (0, 0) - should set bit 0 of byte 0 (buffer) to 1
  display.draw_pixel(0, 0, true);
  EXPECT_EQ(display.get_buffer(), 0x01); // <-- Index added here

  // Out of bounds drawing should be ignored without crashing or changing state
  display.draw_pixel(-1, 0, true);
  display.draw_pixel(128, 0, true);
  display.draw_pixel(0, 32, true);
}

TEST(HalDisplayTest, RenderCountIncrement) {
  HalDisplay display;
  display.init(6, 7);

  EXPECT_EQ(display.get_render_count(), 0);
  display.render();
  EXPECT_EQ(display.get_render_count(), 1);
  display.render();
  EXPECT_EQ(display.get_render_count(), 2);
}