#include <gtest/gtest.h>
#include "core/modulation_engine.hpp"

using namespace phasebox::core;

TEST(MainEngineTest, TickAndMessageGeneration) {
    ModulationEngine engine;
    engine.set_bpm(120.0f);

    auto msgs = engine.tick(1000000);
    ASSERT_GE(msgs.size(), 2u);

    const auto& msg0 = msgs;
    const auto& msg1 = msgs;

    EXPECT_EQ(msg0.status, 0xB0);
    EXPECT_EQ(msg0.data1, 14);

    EXPECT_EQ(msg1.status, 0xB1);
    EXPECT_EQ(msg1.data1, 15);
}
