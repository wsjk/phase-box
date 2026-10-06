#include "ui/ui_controller.hpp"
#include <cstdio>

namespace phasebox::ui {

void UIController::init() { preset_manager_.init(); }

void UIController::handle_encoder_input(int delta, bool button_pressed,
                                        uint32_t hold_time_ms) {
  // Detect 1-second long press for "Hold-to-Save" gesture
  if (button_pressed && hold_time_ms >= 1000) {
    current_state_ = UIState::PresetSave;
    if (!save_triggered_) {
      storage::PresetPatch patch{};
      preset_manager_.save_preset(active_slot_, patch);
      save_triggered_ = true;
    }
    return;
  }

  if (!button_pressed) {
    save_triggered_ = false;
  }

  // Encoder navigation state machine
  if (delta != 0) {
    if (current_state_ == UIState::LiveTelemetry) {
      current_state_ = UIState::ParameterEdit;
    }
  }
}

void UIController::render_frame() {
  // 128x32 OLED screen rendering across 4 live states
  switch (current_state_) {
  case UIState::LiveTelemetry:
    // Render: "[SINE] CH1 BPM:120 EXT | CC#14 Filter VAL:104"
    break;

  case UIState::ParameterEdit:
    // Render: "EDITING: CH1 TARGET CC > CC 14: Feedback"
    break;

  case UIState::PresetSave:
    // Render: "HOLD TO SAVE [ACTIVE] Slot 1: PATCH 01"
    break;

  case UIState::ClockAlign:
    // Render: "TAP TEMPO / SYNC BPM: 120.0 >> PHASE RESET"
    break;
  }
}

} // namespace phasebox::ui