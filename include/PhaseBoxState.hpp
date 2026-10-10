#pragma once
#include "PhaseOscillator.hpp"
struct PhaseBoxState {
    PhaseOscillator osc;
    int active_page = 0;
    bool button_state = false;
    float adc_expression = 0.0f;
    int midi_channel = 1;
    int midi_cc_num = 16;
    int last_sent_cc_val = -1;
};
