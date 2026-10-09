#include <gtest/gtest.h>
#include "core/modulation_engine.hpp"

using namespace phasebox::core;

TEST(MainEngineTest, TickAndMessageGeneration) {
    ModulationEngine engine;
    engine.set_bpm(120.0f);

    auto msgs = engine.tick(1000000);
    ASSERT_GE(msgs.size(), 2u);

    const MidiMessage& m0 = msgs.at(0);
    const MidiMessage& m1 = msgs.at(1);

    EXPECT_EQ(m0.status, 0xB0);
    EXPECT_EQ(m0.data1, 14);

    EXPECT_EQ(m1.status, 0xB1);
    EXPECT_EQ(m1.data1, 15);
}
