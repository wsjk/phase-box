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
    float val = shapeValue(t);
    val *= (0.5f + 0.5f * adc_expression);
    return (val * 0.5f) + 0.5f;
}

float PhaseOscillator::shapeValue(float t) const {
    switch (shape) {
        case SINE: return std::sin(t);
        case SAW: return 1.0f - (t / static_cast<float>(M_PI));
        case SQUARE: return (t < static_cast<float>(M_PI)) ? 1.0f : -1.0f;
        case TRIANGLE: {
            float norm = t / (2.0f * static_cast<float>(M_PI));
            return 2.0f * std::abs(2.0f * (norm - std::floor(norm + 0.5f))) - 1.0f;
        }
    }
    return 0.0f;
}

float PhaseOscillator::normalizedAt(float p) const {
    const float two_pi = 2.0f * static_cast<float>(M_PI);
    float modulator = std::sin(p * pm_ratio) * pm_depth;
    float t = std::fmod(p + phase_offset + modulator, two_pi);
    if (t < 0.0f) t += two_pi;
    return (shapeValue(t) * 0.5f) + 0.5f;
}
