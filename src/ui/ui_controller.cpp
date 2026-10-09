#include "ui/ui_controller.hpp"
#include "core/modulation_engine.hpp"
#include <algorithm>

namespace phasebox::ui {

void UIController::init() {
    m_current_screen = Screen::DASHBOARD;
    m_coarse_mode = false;
    m_last_turn_us = 0;
}

void UIController::handle_encoder_turn(int delta, uint32_t current_time_us) {
    if (delta == 0) return;

    if (m_current_screen == Screen::BPM_SETTING) {
        handle_bpm_encoder_input(delta, current_time_us);
    } else if (m_current_screen == Screen::PRESET_MENU) {
        // Preset navigation
        m_selected_preset_slot = std::clamp(m_selected_preset_slot + delta, 1, 8);
    }
}

void UIController::handle_bpm_encoder_input(int delta, uint32_t current_time_us) {
    uint32_t interval_ms = 100;
    if (m_last_turn_us > 0 && current_time_us > m_last_turn_us) {
        interval_ms = (current_time_us - m_last_turn_us) / 1000;
    }
    m_last_turn_us = current_time_us;

    float current_bpm = m_engine.get_bpm();
    float step = 0.1f;

    // Dynamic velocity acceleration
    if (m_coarse_mode) {
        step = (interval_ms < 30) ? 5.0f : 1.0f;
    } else {
        step = (interval_ms < 30) ? 1.0f : 0.1f;
    }

    float new_bpm = current_bpm + (delta * step);

    // Enforce bounds: 0.1 BPM min to 300.0 BPM max
    new_bpm = std::clamp(new_bpm, 0.1f, 300.0f);

    m_engine.set_bpm(new_bpm);
}

void UIController::handle_encoder_click() {
    if (m_current_screen == Screen::DASHBOARD) {
        m_current_screen = Screen::BPM_SETTING;
    } else if (m_current_screen == Screen::BPM_SETTING) {
        // Toggle between Coarse and Fine adjustment modes on BPM screen
        m_coarse_mode = !m_coarse_mode;
    }
}

void UIController::handle_tap_tempo(uint32_t tap_time_us) {
    static uint32_t last_tap_us = 0;

    if (last_tap_us > 0 && tap_time_us > last_tap_us) {
        uint32_t interval_us = tap_time_us - last_tap_us;
        uint32_t interval_ms = interval_us / 1000;

        // Valid tap tempo window: 200 ms (300 BPM) to 3000 ms (20 BPM)
        if (interval_ms >= 200 && interval_ms <= 3000) {
            float calculated_bpm = 60000.0f / static_cast<float>(interval_ms);
            m_engine.set_bpm(calculated_bpm);
            m_engine.reset_phase(); // Align phase accumulator to tap
        }
    }
    last_tap_us = tap_time_us;
}

} // namespace phasebox::ui
