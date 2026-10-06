#pragma once
#include <cstdint>
#include <array>
#include <algorithm>

namespace phasebox::core {

/**
 * @brief Multi-Destination Macro Engine
 * 
 * Maps expression pedal analog voltage (via HalAdc) to a weighted multi-parameter
 * scaling matrix across all 4 LFO channels.
 */
class MacroEngine {
public:
    MacroEngine() {
        weights_.fill(0.0f); // Default: expression pedal inactive (weight = 0.0)
    }

    void set_expression_value(float value) {
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;
        expression_value_ = value;
    }

    float get_expression_value() const {
        return expression_value_;
    }

    void set_weight(uint8_t lfo_index, float weight) {
        if (lfo_index < weights_.size()) {
            if (weight < -1.0f) weight = -1.0f;
            if (weight > 1.0f) weight = 1.0f;
            weights_[lfo_index] = weight;
        }
    }

    float get_weight(uint8_t lfo_index) const {
        if (lfo_index < weights_.size()) {
            return weights_[lfo_index];
        }
        return 0.0f;
    }

    /**
     * @brief Apply macro scaling to raw 7-bit LFO value.
     * @param lfo_index Target LFO index (0 to 3).
     * @param base_value Raw 7-bit LFO value (0 to 127).
     * @return Scaled and clamped 7-bit MIDI value.
     */
    uint8_t apply(uint8_t lfo_index, uint8_t base_value) const {
        if (lfo_index >= weights_.size()) return base_value;
        float weight = weights_[lfo_index];
        if (weight == 0.0f) return base_value;

        // Bipolar weighted modulation around expression pedal position
        float offset = (expression_value_ - 0.5f) * 127.0f * weight;
        float scaled = static_cast<float>(base_value) + offset;

        if (scaled < 0.0f) scaled = 0.0f;
        if (scaled > 127.0f) scaled = 127.0f;

        return static_cast<uint8_t>(scaled);
    }

private:
    float expression_value_{0.0f};
    std::array<float, 4> weights_{};
};

} // namespace phasebox::core
