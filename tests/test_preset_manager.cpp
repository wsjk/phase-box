#include <gtest/gtest.h>
#include "storage/preset_manager.hpp"
#include <cstdio>

using namespace phasebox::storage;

TEST(PresetManagerTest, SlotValidation) {
    PresetManager pm;
    PresetPatch patch;

    EXPECT_FALSE(pm.save_preset(0, patch)); // Slot 0 invalid
    EXPECT_FALSE(pm.save_preset(9, patch)); // Slot 9 invalid
    EXPECT_TRUE(pm.save_preset(1, patch));  // Slot 1 valid
}

TEST(PresetManagerTest, SaveAndLoadPresetCycle) {
    PresetManager pm;

    PresetPatch original_patch;
    original_patch.bpm = 142.5f;
    original_patch.waveforms = 2; // Square wave
    original_patch.target_ccs = 74; // Filter Cutoff CC
    original_patch.macro_weights = 0.85f;

    // Save to Slot 2
    EXPECT_TRUE(pm.save_preset(2, original_patch));

    // Load back from Slot 2
    PresetPatch loaded_patch{};
    EXPECT_TRUE(pm.load_preset(2, loaded_patch));

    EXPECT_EQ(loaded_patch.version, 1);
    EXPECT_FLOAT_EQ(loaded_patch.bpm, 142.5f);
    EXPECT_EQ(loaded_patch.waveforms, 2);
    EXPECT_EQ(loaded_patch.target_ccs, 74);
    EXPECT_FLOAT_EQ(loaded_patch.macro_weights, 0.85f);

    // Clean up test file
    std::remove("patch2.bin");
}
