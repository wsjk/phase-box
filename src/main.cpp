#include "ui/ui_controller.hpp"

static phasebox::ui::UIController g_ui_controller;

void core1_main() {
  g_ui_controller.init();

  while (true) {
    // Poll encoder hardware inputs
    int delta = 0;        // Read quadrature encoder delta
    bool btn = false;     // Read encoder button state
    uint32_t hold_ms = 0; // Measure press duration

    g_ui_controller.handle_encoder_input(delta, btn, hold_ms);

    // Render display at 30 FPS (33 ms frame delay)
    g_ui_controller.render_frame();
    sleep_ms(33);
  }
}