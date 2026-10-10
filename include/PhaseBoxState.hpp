#pragma once
#include "PhaseOscillator.hpp"
#include "TuringMachine.hpp"
#include "MacroEngine.hpp"

struct PhaseBoxState {
    PhaseOscillator osc;
    TuringMachine turing;
    MacroEngine macros;
    int active_page = 0;        // 0: Wave, 1: BPM, 2: Phase, 3: Chan, 4: CC, 5: PM, 6: Mute
    bool button_state = false;
    float adc_expression = 0.0f;
    int midi_channel = 1;       
    int midi_cc_num = 16;       
    int last_sent_cc_val = -1;
    float last_phase = 0.0f;
};
