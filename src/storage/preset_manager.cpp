#include "storage/preset_manager.hpp"
#include <cstdio>
#include <cstring>

namespace phasebox::storage {

bool PresetManager::init() {
    // Hardware LittleFS initialization happens here on Pico build
    return true;
}

bool PresetManager::save_preset(uint8_t slot, const PresetPatch& patch) {
    if (!is_valid_slot(slot)) return false;

    char filename[64];
    std::snprintf(filename, sizeof(filename), "patch%d.bin", slot);

    FILE* file = std::fopen(filename, "wb");
    if (!file) return false;

    size_t written = std::fwrite(&patch, sizeof(PresetPatch), 1, file);
    std::fclose(file);

    return written == 1;
}

bool PresetManager::load_preset(uint8_t slot, PresetPatch& patch) {
    if (!is_valid_slot(slot)) return false;

    char filename[64];
    std::snprintf(filename, sizeof(filename), "patch%d.bin", slot);

    FILE* file = std::fopen(filename, "rb");
    if (!file) return false;

    size_t read_bytes = std::fread(&patch, sizeof(PresetPatch), 1, file);
    std::fclose(file);

    // Validate version tag
    if (read_bytes != 1 || patch.version != 1) {
        return false;
    }

    return true;
}

} // namespace phasebox::storage
