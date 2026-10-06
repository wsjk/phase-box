#include <gtest/gtest.h>
#include "core/modulation_engine.hpp"

using namespace phasebox::core;

TEST(MainEngineTest, TickAndMessageGeneration) {
    ModulationEngine engine;
    engine.set_bpm(120.0f);

    auto msgs = engine.tick(1000000);
    ASSERT_GE(msgs.size(), 2u);

    EXPECT_EQ(msgs.at(0).status, 0xB0);
    EXPECT_EQ(msgs.at(0).data1, 14);

    EXPECT_EQ(msgs.at(1).status, 0xB1);
    EXPECT_EQ(msgs.at(1).data1, 15);
}
