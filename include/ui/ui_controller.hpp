#ifndef PHASEBOX_UI_CONTROLLER_HPP
#define PHASEBOX_UI_CONTROLLER_HPP

#include "storage/preset_manager.hpp"
#include <cstdint>

namespace phasebox::ui {

enum class UIState {
  LiveTelemetry = 0, // State 1: Active waveform, BPM, CC values
  ParameterEdit = 1, // State 2: Target CC mapping & scaling weights
  PresetSave = 2,    // State 3: Hold-to-Save LittleFS flash write
  ClockAlign = 3     // State 4: Tap tempo & Phase Reset notification
};

class UIController {
public:
  UIController() = default;

  void init();

  // Process rotary encoder turns and button presses
  void handle_encoder_input(int delta, bool button_pressed,
                            uint32_t hold_time_ms);

  // Render 128x32 OLED frame (~30 FPS update rate)
  void render_frame();

  UIState get_state() const { return current_state_; }

private:
  UIState current_state_ = UIState::LiveTelemetry;
  uint8_t active_slot_ = 1;
  bool save_triggered_ = false;
  storage::PresetManager preset_manager_;
};

} // namespace phasebox::ui

#endif // PHASEBOX_UI_CONTROLLER_HPP