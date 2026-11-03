#pragma once
#include <vector>
#include "PhaseOscillator.hpp"
#include "TuringMachine.hpp"
#include "MacroEngine.hpp"

struct PhaseBoxState {
    PhaseOscillator osc;
    TuringMachine turing;
    MacroEngine macros;
    int active_page = 0;        // 0: Wave, 1: BPM, 2: Phase, 3: Chan, 4: CC, 5: PM, 6: Mute, 7: Mac sel, 8: Mac wgt, 9: Mac div, 10: Turing mix
    bool button_state = false;
    float adc_expression = 0.0f;
    // Legacy global values. P4/P5 now edit the *active destination's* channel/CC instead;
    // these stay only so preset files keep their layout.
    int midi_channel = 1;
    int midi_cc_num = 16;
    int last_sent_cc_val = -1;
    float last_phase = 0.0f;

    float turing_norm = 0.5f;   // latest Turing output (0..1), updated once per master cycle
    float turing_mix = 0.0f;    // P11: 0 = LFO only, 1 = Turing sequence only

    /// Every setting back to its power-up default: oscillator (wave, BPM, phase, PM), Turing (mutation, mix,
    /// pattern), and every destination (channel, CC, weight, offset, multiplier). All LFOs restart at 0 degrees.
    /// Not touched: the expression pedal (an input, not a setting), the current page and the preset slot.
    void reset_parameters() {
        osc = PhaseOscillator();
        turing.reset();
        macros = MacroEngine();
        turing_norm = 0.5f;
        turing_mix = 0.0f;
        midi_channel = 1;
        midi_cc_num = 16;
        last_phase = 0.0f;
        last_sent_cc_val = -1;
    }

    /// Step the Turing machine once (also what the Mutate footswitch / [M] key does).
    void step_turing() { turing_norm = static_cast<float>(turing.step()) / 127.0f; }

    /// Footswitch 1: put every LFO back at 0 degrees so the next beat lines up with the tap.
    void restart_phase() {
        osc.phase = 0.0f;
        last_phase = 0.0f;
        macros.reset_phases();
    }

    /// Advance the master oscillator; the Turing machine steps on every master cycle.
    void advance(float dt) {
        osc.update(dt);
        if (osc.phase < last_phase) step_turing();
        last_phase = osc.phase;
    }

    /// Evaluate every macro destination with the current oscillator shape/phase/PM and Turing mix.
    std::vector<MacroEngine::EvaluatedMIDI> evaluateMacros(float dt) {
        MacroEngine::Source src;
        src.osc = &osc;
        src.turing_norm = turing_norm;
        src.turing_mix = turing_mix;
        return macros.evaluate(osc.phase, dt, adc_expression, src);
    }
};
