#pragma once
#include <cmath>
#include <algorithm>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
class PhaseOscillator {
public:
    enum WaveShape { SINE = 0, SAW = 1, SQUARE = 2, TRIANGLE = 3 };
    WaveShape shape = SINE;
    float bpm = 120.0f;
    float phase_offset = 0.0f;
    float phase = 0.0f;
    float pm_depth = 0.0f;
    float pm_ratio = 2.0f;
    void update(float dt);
    float evaluateCC(float t_offset, float adc_expression);

    /// Raw waveform for t in [0, 2*pi): returns [-1, 1] for the current `shape`.
    float shapeValue(float t) const;
    /// Waveform at an arbitrary phase `p` with this oscillator's phase offset and phase
    /// modulation applied. Returns [0, 1]. Used by MacroEngine so P1/P3/P6 reach MIDI.
    float normalizedAt(float p) const;
};
