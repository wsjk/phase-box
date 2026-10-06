#include <gtest/gtest.h>
#include "core/modulation_engine.hpp"

using namespace phasebox::core;

TEST(MainEngineTest, TickAndMessageGeneration) {
    ModulationEngine engine;
    engine.set_bpm(120.0f);

    auto msgs = engine.tick(1000000);
    ASSERT_GE(msgs.size(), 2u);

    // Index into individual vector elements (msgs and msgs)
    EXPECT_EQ(msgs.status, 0xB0); // LFO 0 on Channel 1 (CC 0xB0)
    EXPECT_EQ(msgs.data1, 14);   // Target CC #14

    EXPECT_EQ(msgs.status, 0xB1); // LFO 1 on Channel 2 (CC 0xB1)
    EXPECT_EQ(msgs.data1, 15);   // Target CC #15
}
