// FlashPresetStorage on a RAM fake of NOR flash: erase sets 0xFF, programming can only clear bits.
#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include "FlashPresetStorage.hpp"

namespace {
struct FakeFlash : FlashDevice {
    std::vector<uint8_t> mem = std::vector<uint8_t>(FlashPresetStorage::REGION_BYTES, 0xFF);
    int erases = 0, programs = 0;
    int tear_program = -1;           // >= 0: the program with this number only half lands and reports failure
    bool ok = true;
    bool available() const override { return ok; }
    void read(uint32_t off, uint8_t* out, size_t n) override { std::memcpy(out, mem.data() + off, n); }
    bool erase_sector(uint32_t off) override {
        EXPECT_EQ(off % SECTOR_BYTES, 0u);
        ++erases;
        std::memset(mem.data() + off, 0xFF, SECTOR_BYTES);
        return true;
    }
    bool program(uint32_t off, const uint8_t* d, size_t n) override {
        EXPECT_EQ(off % PAGE_BYTES, 0u);
        EXPECT_EQ(n % PAGE_BYTES, 0u);
        const bool tear = programs == tear_program;
        ++programs;
        const size_t lands = tear ? n / 2 : n;
        for (size_t i = 0; i < lands; ++i) mem[off + i] &= d[i];   // NOR: bits only go 1 -> 0
        return !tear;
    }
};

std::vector<uint8_t> bytes(uint8_t fill, size_t n = 366) { return std::vector<uint8_t>(n, fill); }

std::vector<uint8_t> read_slot(FlashPresetStorage& st, int slot) {
    uint8_t buf[512];
    const size_t n = st.read(slot, buf, sizeof buf);
    return std::vector<uint8_t>(buf, buf + n);
}
} // namespace

TEST(FlashStorage, EmptyFlashHasNoPresets) {
    FakeFlash f; FlashPresetStorage st(f);
    for (int s = 1; s <= 8; ++s) EXPECT_TRUE(read_slot(st, s).empty());
}

TEST(FlashStorage, WhatIsWrittenComesBackPerSlotAfterAPowerCycle) {
    FakeFlash f; FlashPresetStorage st(f);
    for (int s = 1; s <= 8; ++s) { auto d = bytes(static_cast<uint8_t>(s)); ASSERT_TRUE(st.write(s, d.data(), d.size())); }
    FlashPresetStorage after_power_cycle(f);
    for (int s = 1; s <= 8; ++s) EXPECT_EQ(read_slot(after_power_cycle, s), bytes(static_cast<uint8_t>(s)));
}

TEST(FlashStorage, TheNewestSaveWinsAndASectorIsErasedOnlyEveryEighthSave) {
    FakeFlash f; FlashPresetStorage st(f);
    for (int i = 1; i <= 40; ++i) {
        auto d = bytes(static_cast<uint8_t>(i), 100 + i);
        ASSERT_TRUE(st.write(3, d.data(), d.size()));
        ASSERT_EQ(read_slot(st, 3), d) << "after save " << i;
    }
    EXPECT_EQ(f.erases, 4);                 // saves 9, 17, 25 and 33 start over in the other sector
    EXPECT_TRUE(read_slot(st, 2).empty());  // neighbours untouched
    EXPECT_TRUE(read_slot(st, 4).empty());
}

TEST(FlashStorage, ATornWriteKeepsThePreviousSave) {
    FakeFlash f; FlashPresetStorage st(f);
    auto a = bytes(0x11); ASSERT_TRUE(st.write(1, a.data(), a.size()));
    f.tear_program = f.programs;            // power lost half way through the next save
    auto b = bytes(0x22); EXPECT_FALSE(st.write(1, b.data(), b.size()));
    EXPECT_EQ(read_slot(st, 1), a);
    auto c = bytes(0x33); ASSERT_TRUE(st.write(1, c.data(), c.size()));   // the next save still works
    EXPECT_EQ(read_slot(st, 1), c);
}

TEST(FlashStorage, ATornWriteIntoAFreshSectorKeepsThePreviousSave) {
    FakeFlash f; FlashPresetStorage st(f);
    for (int i = 1; i <= 8; ++i) { auto d = bytes(static_cast<uint8_t>(i)); ASSERT_TRUE(st.write(1, d.data(), d.size())); }
    f.tear_program = f.programs;            // save 9 erases the other sector, then tears
    auto d = bytes(9); EXPECT_FALSE(st.write(1, d.data(), d.size()));
    EXPECT_EQ(read_slot(st, 1), bytes(8));
}

TEST(FlashStorage, ACorruptedRecordIsSkipped) {
    FakeFlash f; FlashPresetStorage st(f);
    auto a = bytes(0x11); ASSERT_TRUE(st.write(1, a.data(), a.size()));
    auto b = bytes(0x22); ASSERT_TRUE(st.write(1, b.data(), b.size()));
    f.mem[FlashPresetStorage::RECORD_BYTES + 100] ^= 0x01;   // a flipped bit in the newest record
    EXPECT_EQ(read_slot(st, 1), a);
}

TEST(FlashStorage, RefusesBadArgumentsAndUnusableFlash) {
    FakeFlash f; FlashPresetStorage st(f);
    auto d = bytes(1);
    EXPECT_FALSE(st.write(0, d.data(), d.size()));
    EXPECT_FALSE(st.write(9, d.data(), d.size()));
    EXPECT_FALSE(st.write(1, d.data(), 0));
    EXPECT_FALSE(st.write(1, d.data(), FlashPresetStorage::MAX_DATA + 1));
    ASSERT_TRUE(st.write(1, d.data(), d.size()));
    uint8_t small[10];
    EXPECT_EQ(st.read(1, small, sizeof small), 0u);          // does not fit the caller's buffer
    f.ok = false;
    EXPECT_FALSE(st.available());
    EXPECT_FALSE(st.write(1, d.data(), d.size()));
    EXPECT_TRUE(read_slot(st, 1).empty());
}

TEST(FlashStorage, ARealPresetFitsAndRoundTripsThroughPresetSession) {
    FakeFlash f; FlashPresetStorage st(f);
    PhaseBoxState s;
    uint8_t buf[PresetManager::MAX_BYTES];
    s.macros.destinations.resize(PresetManager::MAX_MACROS);   // the largest preset there can be
    EXPECT_LE(PresetManager::encode(s, buf, sizeof buf), FlashPresetStorage::MAX_DATA);

    PhaseBoxState a;
    {
        PresetSession session(st, 4);
        session.update(a, 0);
        a.osc.bpm = 97.0f;
        session.update(a, 10);
        session.update(a, 10 + PresetSession::AUTOSAVE_QUIET_MS + 1);
    }
    PhaseBoxState back;
    PresetSession session(st, 4);
    ASSERT_TRUE(session.load(back, 0));
    EXPECT_FLOAT_EQ(back.osc.bpm, 97.0f);
}
