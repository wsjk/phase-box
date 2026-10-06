#ifndef PHASEBOX_STORAGE_PRESET_MANAGER_HPP
#define PHASEBOX_STORAGE_PRESET_MANAGER_HPP

#include <cstdint>

namespace phasebox::storage {

struct PresetPatch {
  uint8_t version = 1;
  uint8_t lfo_waveforms; // Sine, Triangle, Square, S&H, Turing
  float lfo_frequencies;
  uint8_t target_channels;
  uint8_t target_ccs;
  float bpm;
  uint8_t reserved;
};

class PresetManager {
public:
  PresetManager() = default;

  // Initializes LittleFS on RP2040 SPI flash
  bool init();

  // Saves active configuration to flash slot (e.g. "patch1.bin")
  bool save_preset(uint8_t slot, const PresetPatch &patch);

  // Loads configuration from flash slot
  bool load_preset(uint8_t slot, PresetPatch &patch);
};

} // namespace phasebox::storage

#endif // PHASEBOX_STORAGE_PRESET_MANAGER_HPP