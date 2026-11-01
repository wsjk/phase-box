// Auto-save (2 s after the last edit), hold-to-save (1 s) and slots, with a fake clock and in-memory storage.
#include <gtest/gtest.h>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include "PresetSession.hpp"

namespace {
struct MemStorage : PresetStorage {
    std::map<int, std::vector<uint8_t>> slots;
    bool fail = false;
    int writes = 0;
    bool write(int slot, const uint8_t* d, size_t n) override {
        ++writes;
        if (fail) return false;
        slots[slot].assign(d, d + n);
        return true;
    }
    size_t read(int slot, uint8_t* out, size_t cap) override {
        auto it = slots.find(slot);
        if (it == slots.end() || it->second.size() > cap) return 0;
        std::memcpy(out, it->second.data(), it->second.size());
        return it->second.size();
    }
};
bool same(const char* a, const char* b) { return a && b && std::strcmp(a, b) == 0; }
float stored_bpm(MemStorage& m, int slot) {
    PhaseBoxState tmp;
    return PresetManager::decode(m.slots[slot].data(), m.slots[slot].size(), tmp) ? tmp.osc.bpm : -1.0f;
}
} // namespace

TEST(AutoSave, NothingIsWrittenWithoutAnEdit) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    for (uint32_t t = 0; t < 30000; t += 16) ps.update(s, t);
    EXPECT_EQ(m.writes, 0);
    EXPECT_FALSE(ps.dirty());
}

TEST(AutoSave, IgnoresStateThatIsNotPartOfAPreset) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    ps.update(s, 0);
    s.active_page = 7; s.adc_expression = 1.0f; s.osc.phase = 3.0f; s.button_state = true; s.turing_norm = 0.9f;
    for (uint32_t t = 16; t < 10000; t += 16) ps.update(s, t);
    EXPECT_EQ(m.writes, 0);
}

TEST(AutoSave, WritesSilentlyAfterTwoQuietSeconds) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    ps.update(s, 1000);
    s.osc.bpm = 140.0f;
    ps.update(s, 1000);
    EXPECT_TRUE(ps.dirty());
    ps.update(s, 2999);
    EXPECT_EQ(m.writes, 0);                    // 1999 ms quiet: not yet
    ps.update(s, 3000);
    EXPECT_EQ(m.writes, 1);                    // 2000 ms quiet: saved
    EXPECT_NEAR(stored_bpm(m, 1), 140.0f, 0.001f);
    EXPECT_FALSE(ps.dirty());
    EXPECT_TRUE(ps.status(3000) == nullptr);   // the guide says auto-save is silent
    ps.update(s, 9000);
    EXPECT_EQ(m.writes, 1);                    // and it does not repeat
}

TEST(AutoSave, EveryEditRestartsTheTimer) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    ps.update(s, 0);
    s.osc.bpm = 130.0f; ps.update(s, 0);
    s.osc.bpm = 135.0f; ps.update(s, 1500);
    ps.update(s, 2500);                        // only 1000 ms since the last edit
    EXPECT_EQ(m.writes, 0);
    ps.update(s, 3500);
    EXPECT_EQ(m.writes, 1);
    EXPECT_NEAR(stored_bpm(m, 1), 135.0f, 0.001f);
}

TEST(AutoSave, SurvivesTheMillisecondCounterWrapping) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    const uint32_t t0 = 0xFFFFFC00u;           // 1024 ms before the 32-bit counter wraps
    ps.update(s, t0);
    s.osc.bpm = 150.0f; ps.update(s, t0);
    ps.update(s, t0 + 1999);
    EXPECT_EQ(m.writes, 0);
    ps.update(s, t0 + 2000);                   // crosses the wrap
    EXPECT_EQ(m.writes, 1);
}

TEST(AutoSave, FailedWriteIsReportedAndRetriedOnlyLater) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    ps.update(s, 0);
    s.osc.bpm = 111.0f; ps.update(s, 0);
    m.fail = true;
    ps.update(s, 2000);
    EXPECT_EQ(m.writes, 1);
    EXPECT_TRUE(same(ps.status(2000), "SAVE FAILED"));
    EXPECT_TRUE(ps.dirty());
    for (uint32_t t = 2016; t < 11000; t += 16) ps.update(s, t);
    EXPECT_EQ(m.writes, 1);                    // no hammering during the 10 s back-off
    ps.update(s, 12000);
    EXPECT_EQ(m.writes, 2);                    // retried, failed again
    m.fail = false;
    ps.update(s, 22000);
    EXPECT_EQ(m.writes, 3);
    EXPECT_FALSE(ps.dirty());
    EXPECT_NEAR(stored_bpm(m, 1), 111.0f, 0.001f);
}

TEST(HoldToSave, SavesAtOneSecondAndShowsSaved) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    ps.update(s, 0);
    s.osc.bpm = 99.0f;
    EXPECT_TRUE(ps.button(true, s, 0) == PresetSession::Event::None);
    EXPECT_TRUE(ps.button(true, s, 999) == PresetSession::Event::None);
    EXPECT_EQ(m.writes, 0);
    EXPECT_TRUE(ps.button(true, s, 1000) == PresetSession::Event::LongPress);
    EXPECT_EQ(m.writes, 1);
    EXPECT_NEAR(stored_bpm(m, 1), 99.0f, 0.001f);
    EXPECT_TRUE(same(ps.status(1000), "SAVED"));
    EXPECT_TRUE(same(ps.status(2499), "SAVED"));
    EXPECT_TRUE(ps.status(2500) == nullptr);   // message clears after 1.5 s
    EXPECT_TRUE(ps.button(true, s, 1500) == PresetSession::Event::None);   // still held: fires once
    EXPECT_EQ(m.writes, 1);
    EXPECT_TRUE(ps.button(false, s, 1600) == PresetSession::Event::None);  // releasing after a long press is not a short press
}

TEST(HoldToSave, ShortPressIsReportedAndDoesNotSave) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    ps.button(true, s, 0);
    EXPECT_TRUE(ps.button(false, s, 300) == PresetSession::Event::ShortPress);
    EXPECT_EQ(m.writes, 0);
    EXPECT_TRUE(ps.status(300) == nullptr);
}

TEST(HoldToSave, ProgressBarAndRearming) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    EXPECT_NEAR(ps.hold_progress(0), 0.0f, 0.0001f);
    ps.button(true, s, 0);
    EXPECT_NEAR(ps.hold_progress(500), 0.5f, 0.001f);
    ps.button(true, s, 1000);
    EXPECT_NEAR(ps.hold_progress(1200), 0.0f, 0.0001f);   // done
    ps.button(false, s, 1300);
    ps.button(true, s, 2000);
    ps.button(true, s, 3000);                              // a second, separate hold saves again
    EXPECT_EQ(m.writes, 2);
}

TEST(HoldToSave, ManualSaveWorksEvenWhileAutoSaveIsBackingOff) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    ps.update(s, 0);
    s.osc.bpm = 77.0f; ps.update(s, 0);
    m.fail = true; ps.update(s, 2000);
    m.fail = false;
    ps.button(true, s, 3000);
    ps.button(true, s, 4000);
    EXPECT_NEAR(stored_bpm(m, 1), 77.0f, 0.001f);
    EXPECT_TRUE(same(ps.status(4000), "SAVED"));
}

TEST(PowerUp, LoadRestoresWhatWasSaved) {
    MemStorage m;
    {
        PresetSession ps(m); PhaseBoxState s;
        ps.update(s, 0);
        s.osc.bpm = 133.0f; s.turing_mix = 0.4f; s.macros.destinations[1].clock_mult = 3.0f;
        ps.update(s, 0); ps.update(s, 2000);
    }
    PresetSession ps(m); PhaseBoxState s2;
    EXPECT_TRUE(ps.load(s2, 0));
    EXPECT_NEAR(s2.osc.bpm, 133.0f, 0.001f);
    EXPECT_NEAR(s2.turing_mix, 0.4f, 0.001f);
    EXPECT_NEAR(s2.macros.destinations[1].clock_mult, 3.0f, 0.001f);
    EXPECT_TRUE(ps.status(0) == nullptr);       // power-up restore is silent
    ps.update(s2, 100);
    EXPECT_FALSE(ps.dirty());
    EXPECT_EQ(m.writes, 1);                     // loading does not trigger a write
}

TEST(PowerUp, CorruptPresetIsReportedAndLeavesSettingsAlone) {
    MemStorage m; PhaseBoxState s;
    {   PresetSession ps(m); ps.update(s, 0); s.osc.bpm = 133.0f; ps.update(s, 0); ps.update(s, 2000); }
    m.slots[1][10] ^= 0xFF;                     // flip a byte: CRC no longer matches
    PhaseBoxState fresh; fresh.osc.bpm = 120.0f;
    PresetSession ps(m);
    EXPECT_FALSE(ps.load(fresh, 5000));
    EXPECT_TRUE(same(ps.status(5000), "BAD PRESET"));
    EXPECT_NEAR(fresh.osc.bpm, 120.0f, 0.001f);
}

TEST(Slots, SwitchingFlushesPendingEditsLoadsTargetAndKeepsEmptySlotsClean) {
    MemStorage m; PresetSession ps(m); PhaseBoxState s;
    ps.update(s, 0);
    s.osc.bpm = 100.0f; ps.update(s, 0);
    EXPECT_TRUE(ps.select_slot(2, s, 500));      // only 500 ms after the edit
    EXPECT_NEAR(stored_bpm(m, 1), 100.0f, 0.001f);       // edit was not lost: flushed to slot 1
    EXPECT_EQ(ps.slot(), 2);
    EXPECT_TRUE(same(ps.status(500), "EMPTY"));
    EXPECT_NEAR(s.osc.bpm, 100.0f, 0.001f);              // settings carried over into the empty slot
    for (uint32_t t = 516; t < 8000; t += 16) ps.update(s, t);
    EXPECT_EQ(m.slots.count(2), 0u);                     // nothing is written until you edit or hold-save

    s.osc.bpm = 90.0f; ps.update(s, 8000); ps.update(s, 10000);
    EXPECT_NEAR(stored_bpm(m, 2), 90.0f, 0.001f);        // an edit auto-saves into slot 2

    EXPECT_TRUE(ps.select_slot(1, s, 11000));
    EXPECT_NEAR(s.osc.bpm, 100.0f, 0.001f);              // slot 1 comes back as saved
    EXPECT_TRUE(same(ps.status(11000), "LOADED"));
    EXPECT_FALSE(ps.select_slot(0, s, 11000));
    EXPECT_FALSE(ps.select_slot(9, s, 11000));
}

TEST(Slots, WorksWithRealFilesToo) {
    PresetManager::path_prefix = "./";
    FilePresetStorage files;
    PresetSession ps(files, 3);
    PhaseBoxState s;
    ps.update(s, 0);
    s.osc.bpm = 144.0f; ps.update(s, 0); ps.update(s, 2000);
    PhaseBoxState back;
    EXPECT_TRUE(PresetManager::loadPreset(3, back));     // the older whole-file API reads the same file
    EXPECT_NEAR(back.osc.bpm, 144.0f, 0.001f);
    std::remove("./preset_3.bin");
}
