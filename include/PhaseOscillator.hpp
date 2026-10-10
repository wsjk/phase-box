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
};
