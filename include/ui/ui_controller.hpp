#pragma once
#include "core/modulation_engine.hpp"
#include "hal/hal_display.hpp"
#include "hal/hal_flash.hpp"
#include <cstdint>
#include <string>
#include <cstdio>
#include <cstring>

namespace phasebox::ui {

enum class UIState {
    Telemetry = 0,
    ParameterEdit,
    HoldToSave,
    ClockSync
};

enum class EditParam {
    TargetCC = 0,
    Channel,
    Waveform,
    MutationProb,
    Count
};

struct PresetData {
    uint32_t magic{0x50485342}; // "PHSB"
    uint8_t version{1};
    float bpm{120.0f};
    struct LfoSettings {
        uint8_t waveform{0};
        uint8_t mutation_prob{20};
        uint8_t channel{1};
        uint8_t cc_number{14};
    } lfos[4];
    char name[16]{"Default"};
};

class UIController {
public:
    static constexpr uint32_t HOLD_TO_SAVE_THRESHOLD_US = 1000000; // 1 second
    static constexpr uint32_t CLOCK_SYNC_DISPLAY_TIMEOUT_US = 1500000; // 1.5 seconds

    UIController(core::ModulationEngine& engine, hal::HalDisplay& display, hal::HalFlash* flash = nullptr)
        : engine_(engine), display_(display), flash_(flash) {}

    void handle_input(int32_t encoder_delta, bool encoder_button, uint32_t current_time_us) {
        // Track button press duration for Hold-to-Save gesture
        if (encoder_button) {
            if (!last_button_state_) {
                button_press_start_us_ = current_time_us;
            } else if (current_time_us - button_press_start_us_ >= HOLD_TO_SAVE_THRESHOLD_US) {
                if (state_ != UIState::HoldToSave) {
                    previous_state_ = state_;
                    state_ = UIState::HoldToSave;
                }
            }
        } else {
            if (last_button_state_) {
                // Button released
                if (state_ == UIState::HoldToSave) {
                    // Release after hold-to-save -> Commit preset write to flash
                    save_current_preset(selected_slot_);
                    state_ = previous_state_;
                } else if (current_time_us - button_press_start_us_ < HOLD_TO_SAVE_THRESHOLD_US) {
                    // Short click -> toggle states or select parameter
                    handle_short_click();
                }
            }
        }
        last_button_state_ = encoder_button;

        // Check clock sync display timeout
        if (state_ == UIState::ClockSync && (current_time_us - clock_sync_start_us_ >= CLOCK_SYNC_DISPLAY_TIMEOUT_US)) {
            state_ = previous_state_;
        }

        // Handle rotary encoder turns according to active state
        if (encoder_delta != 0) {
            handle_encoder_rotation(encoder_delta);
        }
    }

    void trigger_clock_sync_event(uint32_t current_time_us) {
        if (state_ != UIState::HoldToSave) {
            previous_state_ = state_;
            state_ = UIState::ClockSync;
            clock_sync_start_us_ = current_time_us;
        }
    }

    void render(uint32_t current_time_us) {
        display_.clear();

        switch (state_) {
            case UIState::Telemetry:
                render_telemetry_view();
                break;
            case UIState::ParameterEdit:
                render_parameter_edit_view();
                break;
            case UIState::HoldToSave:
                render_hold_to_save_view();
                break;
            case UIState::ClockSync:
                render_clock_sync_view();
                break;
        }

        display_.update();
    }

    bool save_current_preset(uint8_t slot) {
        if (flash_ == nullptr) return false;

        PresetData preset;
        preset.bpm = engine_.get_clock_manager().get_bpm();
        std::snprintf(preset.name, sizeof(preset.name), "Slot %u", slot + 1);

        for (size_t i = 0; i < 4; ++i) {
            preset.lfos[i].channel = static_cast<uint8_t>(i + 1);
            preset.lfos[i].cc_number = static_cast<uint8_t>(14 + i);
            preset.lfos[i].waveform = 0;
            preset.lfos[i].mutation_prob = 20;
        }

        return flash_->save_preset(slot, reinterpret_cast<const uint8_t*>(&preset), sizeof(preset));
    }

    bool load_preset(uint8_t slot) {
        if (flash_ == nullptr) return false;

        PresetData preset;
        if (!flash_->load_preset(slot, reinterpret_cast<uint8_t*>(&preset), sizeof(preset))) {
            return false;
        }

        if (preset.magic == 0x50485342 && preset.bpm > 0.0f) {
            engine_.set_bpm(preset.bpm);
            for (size_t i = 0; i < 4; ++i) {
                engine_.get_router().set_target_channel(static_cast<uint8_t>(i), preset.lfos[i].channel);
                engine_.get_router().set_target_cc(static_cast<uint8_t>(i), preset.lfos[i].cc_number);
            }
            return true;
        }
        return false;
    }

    UIState get_current_state() const { return state_; }
    void set_state(UIState state) { state_ = state; }
    uint8_t get_selected_lfo() const { return selected_lfo_; }
    uint8_t get_selected_slot() const { return selected_slot_; }
    EditParam get_selected_param() const { return selected_param_; }

private:
    void handle_short_click() {
        if (state_ == UIState::Telemetry) {
            state_ = UIState::ParameterEdit;
        } else if (state_ == UIState::ParameterEdit) {
            // Cycle through editable parameters
            int next_param = (static_cast<int>(selected_param_) + 1);
            if (next_param >= static_cast<int>(EditParam::Count)) {
                state_ = UIState::Telemetry;
                selected_param_ = EditParam::TargetCC;
            } else {
                selected_param_ = static_cast<EditParam>(next_param);
            }
        }
    }

    void handle_encoder_rotation(int32_t delta) {
        switch (state_) {
            case UIState::Telemetry: {
                // Change monitored LFO channel (0 to 3)
                int new_lfo = static_cast<int>(selected_lfo_) + delta;
                if (new_lfo < 0) new_lfo = 0;
                if (new_lfo > 3) new_lfo = 3;
                selected_lfo_ = static_cast<uint8_t>(new_lfo);
                break;
            }
            case UIState::ParameterEdit: {
                if (selected_param_ == EditParam::TargetCC) {
                    uint8_t cur_cc = 14 + selected_lfo_;
                    int next_cc = static_cast<int>(cur_cc) + delta;
                    if (next_cc >= 0 && next_cc <= 127) {
                        engine_.get_router().set_target_cc(selected_lfo_, static_cast<uint8_t>(next_cc));
                    }
                } else if (selected_param_ == EditParam::Channel) {
                    uint8_t cur_ch = selected_lfo_ + 1;
                    int next_ch = static_cast<int>(cur_ch) + delta;
                    if (next_ch >= 1 && next_ch <= 16) {
                        engine_.get_router().set_target_channel(selected_lfo_, static_cast<uint8_t>(next_ch));
                    }
                }
                break;
            }
            case UIState::HoldToSave: {
                int next_slot = static_cast<int>(selected_slot_) + delta;
                if (next_slot < 0) next_slot = 0;
                if (next_slot > 15) next_slot = 15;
                selected_slot_ = static_cast<uint8_t>(next_slot);
                break;
            }
            case UIState::ClockSync:
                break;
        }
    }

    void render_telemetry_view() {
        char line1[32];
        const char* clk_str = (engine_.get_clock_manager().get_clock_source() == core::ClockSource::ExternalMIDI) ? "EXT" : "INT";
        std::snprintf(line1, sizeof(line1), "[SINE] CH%u BPM:%u %s", selected_lfo_ + 1,
                      static_cast<unsigned int>(engine_.get_clock_manager().get_bpm()), clk_str);
        display_.draw_string(0, 0, line1);

        display_.draw_string(0, 10, "/^\\  /^\\  /^\\  /^\\");

        char line3[32];
        std::snprintf(line3, sizeof(line3), "CC#%u LFO%u OUT", 14 + selected_lfo_, selected_lfo_ + 1);
        display_.draw_string(0, 22, line3);
    }

    void render_parameter_edit_view() {
        char line1[32];
        std::snprintf(line1, sizeof(line1), "EDIT: CH%u TARGET", selected_lfo_ + 1);
        display_.draw_string(0, 0, line1);

        char line2[32];
        if (selected_param_ == EditParam::TargetCC) {
            std::snprintf(line2, sizeof(line2), "> CC %u: PARAM", 14 + selected_lfo_);
        } else if (selected_param_ == EditParam::Channel) {
            std::snprintf(line2, sizeof(line2), "> CHAN: %u", selected_lfo_ + 1);
        } else {
            std::snprintf(line2, sizeof(line2), "> WAVE: SINE");
        }
        display_.draw_string(0, 12, line2);

        display_.draw_string(0, 24, "Turn/Click Enc");
    }

    void render_hold_to_save_view() {
        display_.draw_string(0, 0, "[HOLD TO SAVE]");
        char line2[32];
        std::snprintf(line2, sizeof(line2), "> Slot %u: PRESET", selected_slot_ + 1);
        display_.draw_string(0, 12, line2);
        display_.draw_string(0, 24, "Release to Write");
    }

    void render_clock_sync_view() {
        display_.draw_string(0, 0, "TAP TEMPO / SYNC");
        char line2[32];
        std::snprintf(line2, sizeof(line2), "BPM: %u [TAP DETECT]", static_cast<unsigned int>(engine_.get_clock_manager().get_bpm()));
        display_.draw_string(0, 12, line2);
        display_.draw_string(0, 24, ">> PHASE RESET TO 0");
    }

    core::ModulationEngine& engine_;
    hal::HalDisplay& display_;
    hal::HalFlash* flash_{nullptr};

    UIState state_{UIState::Telemetry};
    UIState previous_state_{UIState::Telemetry};
    EditParam selected_param_{EditParam::TargetCC};

    uint8_t selected_lfo_{0};
    uint8_t selected_slot_{0};

    bool last_button_state_{false};
    uint32_t button_press_start_us_{0};
    uint32_t clock_sync_start_us_{0};
};

} // namespace phasebox::ui
