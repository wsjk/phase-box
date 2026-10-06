#ifndef PHASEBOX_STORAGE_PRESET_MANAGER_HPP
#define PHASEBOX_STORAGE_PRESET_MANAGER_HPP

#include <cstdint>
#include <array>

namespace phasebox::storage {

#pragma pack(push, 1)
struct PresetPatch {
    uint8_t version{1};
    std::array<uint8_t, 4> waveforms{0, 0, 0, 0};      // Waveform types
    std::array<float, 4> frequencies{1.0f, 1.0f, 1.0f, 1.0f}; // Frequencies in Hz
    std::array<uint8_t, 4> target_channels{1, 2, 3, 4}; // MIDI Channels (1-16)
    std::array<uint8_t, 4> target_ccs{14, 15, 16, 17};  // MIDI CC numbers
    std::array<float, 4> macro_weights{0.0f, 0.0f, 0.0f, 0.0f}; // Expression weights
    float bpm{120.0f};
};
#pragma pack(pop)

class PresetManager {
public:
    PresetManager() = default;

    /**
     * @brief Initialize storage interface.
     */
    bool init();

    /**
     * @brief Save patch configuration to specified slot (1 to 8).
     */
    bool save_preset(uint8_t slot, const PresetPatch& patch);

    /**
     * @brief Load patch configuration from specified slot (1 to 8).
     */
    bool load_preset(uint8_t slot, PresetPatch& patch);

private:
    bool is_valid_slot(uint8_t slot) const { return slot >= 1 && slot <= 8; }
};

} // namespace phasebox::storage

#endif // PHASEBOX_STORAGE_PRESET_MANAGER_HPP
