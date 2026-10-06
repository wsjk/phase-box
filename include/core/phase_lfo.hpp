#pragma once
#include <cstdint>
#include <array>
#include <cmath>

namespace phasebox::core {

enum class Waveform {
    Sine,
    Triangle,
    Square,
    SampleAndHold,
    TuringMutation
};

class PhaseLFO {
public:
    PhaseLFO() {
        init_lut();
    }

    void set_frequency(float hz, uint32_t sample_rate_hz = 1000) {
        if (sample_rate_hz == 0) return;
        phase_increment_ = static_cast<uint32_t>((hz / static_cast<float>(sample_rate_hz)) * 4294967296.0f);
    }

    void set_waveform(Waveform wf) {
        waveform_ = wf;
    }

    uint8_t update() {
        phase_ += phase_increment_;
        
        switch (waveform_) {
            case Waveform::Sine: {
                uint8_t index = static_cast<uint8_t>(phase_ >> 24);
                return sine_lut_[index];
            }
            case Waveform::Triangle: {
                uint32_t top = phase_ >> 24;
                if (top < 128) {
                    return static_cast<uint8_t>(top * 2);
                } else {
                    return static_cast<uint8_t>((255 - top) * 2);
                }
            }
            case Waveform::Square: {
                return (phase_ < 0x80000000) ? 127 : 0;
            }
            case Waveform::SampleAndHold: {
                if (phase_ < last_phase_) {
                    sh_value_ = static_cast<uint8_t>(rng() % 128);
                }
                last_phase_ = phase_;
                return sh_value_;
            }
            case Waveform::TuringMutation: {
                if (phase_ < last_phase_) {
                    bool bit = (turing_shift_reg_ & 0x01) ^ ((turing_shift_reg_ >> 1) & 0x01);
                    if ((rng() % 100) < mutation_probability_) {
                        bit = !bit;
                    }
                    turing_shift_reg_ = (turing_shift_reg_ >> 1) | (bit ? 0x8000 : 0);
                }
                last_phase_ = phase_;
                return static_cast<uint8_t>(turing_shift_reg_ & 0x7F);
            }
        }
        return 0;
    }

    void set_mutation_probability(uint8_t prob_pct) {
        mutation_probability_ = prob_pct;
    }

    uint8_t get_mutation_probability() const {
        return mutation_probability_;
    }

    Waveform get_waveform() const {
        return waveform_;
    }

    void reset_phase() {
        phase_ = 0;
        last_phase_ = 0;
    }

private:
    void init_lut() {
        for (int i = 0; i < 256; ++i) {
            float rad = (static_cast<float>(i) / 256.0f) * 2.0f * 3.14159265358979323846f;
            float val = (std::sin(rad) + 1.0f) * 63.5f;
            sine_lut_[i] = static_cast<uint8_t>(val);
        }
    }

    uint32_t rng() {
        rng_state_ ^= rng_state_ << 13;
        rng_state_ ^= rng_state_ >> 17;
        rng_state_ ^= rng_state_ << 5;
        return rng_state_;
    }

    uint32_t phase_{0};
    uint32_t last_phase_{0};
    uint32_t phase_increment_{0};
    Waveform waveform_{Waveform::Sine};
    std::array<uint8_t, 256> sine_lut_{};
    uint8_t sh_value_{64};
    uint16_t turing_shift_reg_{0xACE1};
    uint8_t mutation_probability_{20};
    uint32_t rng_state_{0x12345678};
};

} // namespace phasebox::core