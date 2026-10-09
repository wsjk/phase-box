#ifndef PHASEBOX_UI_CONTROLLER_HPP
#define PHASEBOX_UI_CONTROLLER_HPP

#include <cstdint>
#include "core/modulation_engine.hpp"

namespace phasebox::ui {

enum class Screen {
    DASHBOARD,
    BPM_SETTING,
    PRESET_MENU
};

class UIController {
public:
    explicit UIController(core::ModulationEngine& engine) : m_engine(engine) {}

    void init();
    void handle_encoder_turn(int delta, uint32_t current_time_us);
    void handle_bpm_encoder_input(int delta, uint32_t current_time_us);
    void handle_encoder_click();
    void handle_tap_tempo(uint32_t tap_time_us);

    Screen get_current_screen() const { return m_current_screen; }
    bool is_coarse_mode() const { return m_coarse_mode; }
    int get_selected_preset_slot() const { return m_selected_preset_slot; }

private:
    core::ModulationEngine& m_engine;
    Screen m_current_screen{Screen::DASHBOARD};
    bool m_coarse_mode{false};
    uint32_t m_last_turn_us{0};
    int m_selected_preset_slot{1};
};

} // namespace phasebox::ui

#endif // PHASEBOX_UI_CONTROLLER_HPP
