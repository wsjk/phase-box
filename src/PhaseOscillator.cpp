#include "PhaseOscillator.hpp"
void PhaseOscillator::update(float dt) {
    float freq = (bpm / 60.0f) * 2.0f;
    phase += freq * dt * static_cast<float>(M_PI);
    if (phase >= 2.0f * static_cast<float>(M_PI)) phase -= 2.0f * static_cast<float>(M_PI);
}
float PhaseOscillator::evaluateCC(float t_offset, float adc_expression) {
    float mod_phase = (phase * pm_ratio) + t_offset;
    float modulator = std::sin(mod_phase) * pm_depth;
    float t = phase + t_offset + phase_offset + modulator;
    t = std::fmod(t, 2.0f * static_cast<float>(M_PI));
    if (t < 0.0f) t += 2.0f * static_cast<float>(M_PI);
    float val = 0.0f;
    switch (shape) {
        case SINE: val = std::sin(t); break;
        case SAW: val = 1.0f - (t / static_cast<float>(M_PI)); break;
        case SQUARE: val = (t < static_cast<float>(M_PI)) ? 1.0f : -1.0f; break;
        case TRIANGLE: {
            float norm = t / (2.0f * static_cast<float>(M_PI));
            val = 2.0f * std::abs(2.0f * (norm - std::floor(norm + 0.5f))) - 1.0f;
            break;
        }
    }
    val *= (0.5f + 0.5f * adc_expression);
    return (val * 0.5f) + 0.5f;
}
