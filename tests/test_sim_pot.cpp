// The simulator's pot-style knob: every page's value maps to a position between two end stops and back.
#include <gtest/gtest.h>
#include <cmath>
#include "SimPot.hpp"

using namespace simui;

namespace {
float at(PhaseBoxState& s, int page, float p, int dest = 0) { int d = dest; simpot::set_norm(s, d, page, p); return simpot::norm(s, d, page); }
}

TEST(SimPot, EndStopsGiveTheMinimumAndMaximumOfEveryPage) {
    PhaseBoxState s; int d = 0;
    simpot::set_norm(s, d, P_WAVE, 0);    EXPECT_EQ(s.osc.shape, PhaseOscillator::SINE);
    simpot::set_norm(s, d, P_WAVE, 1);    EXPECT_EQ(s.osc.shape, PhaseOscillator::TRIANGLE);
    simpot::set_norm(s, d, P_BPM, 0);     EXPECT_NEAR(s.osc.bpm, 40.0f, 0.001f);
    simpot::set_norm(s, d, P_BPM, 1);     EXPECT_NEAR(s.osc.bpm, 300.0f, 0.001f);
    simpot::set_norm(s, d, P_CHAN, 0);    EXPECT_EQ(s.macros.destinations[0].midi_channel, 1);
    simpot::set_norm(s, d, P_CHAN, 1);    EXPECT_EQ(s.macros.destinations[0].midi_channel, 16);
    simpot::set_norm(s, d, P_CC, 0);      EXPECT_EQ(s.macros.destinations[0].cc_number, 0);
    simpot::set_norm(s, d, P_CC, 1);      EXPECT_EQ(s.macros.destinations[0].cc_number, 127);
    simpot::set_norm(s, d, P_PM, 1);      EXPECT_NEAR(s.osc.pm_depth, 5.0f, 0.001f);
    simpot::set_norm(s, d, P_MUTE, 1);    EXPECT_NEAR(s.turing.mutation_prob, 1.0f, 0.001f);
    simpot::set_norm(s, d, P_MAC_WGT, 0); EXPECT_NEAR(s.macros.destinations[0].scale, -2.0f, 0.001f);
    simpot::set_norm(s, d, P_MAC_WGT, 1); EXPECT_NEAR(s.macros.destinations[0].scale, 2.0f, 0.001f);
    simpot::set_norm(s, d, P_MAC_DIV, 0); EXPECT_NEAR(s.macros.destinations[0].clock_mult, 0.125f, 0.0001f);
    simpot::set_norm(s, d, P_MAC_DIV, 1); EXPECT_NEAR(s.macros.destinations[0].clock_mult, 8.0f, 0.0001f);
    simpot::set_norm(s, d, P_TUR_MIX, 1); EXPECT_NEAR(s.turing_mix, 1.0f, 0.001f);
    simpot::set_norm(s, d, P_MAC_SEL, 1); EXPECT_EQ(d, 2);
    simpot::set_norm(s, d, P_MAC_SEL, 0); EXPECT_EQ(d, 0);
}

TEST(SimPot, ThePositionOfAValueAndBackAgreeOnEveryPage) {
    for (int page = 0; page < P_TUR_MIX + 1; ++page) {
        PhaseBoxState s;
        for (float p : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
            const float shown = at(s, page, p);
            const float snap = page == P_WAVE ? 0.17f : page == P_MAC_SEL ? 0.26f : page == P_MAC_DIV ? 0.06f : page == P_CHAN ? 0.04f : 0.02f;   // pages with few positions snap further
            EXPECT_NEAR(shown, p, snap);
            const float again = at(s, page, shown);
            EXPECT_NEAR(again, shown, 0.001f);                    // putting the knob where it is shown changes nothing
        }
    }
}

TEST(SimPot, DefaultsSitWhereTheValueIs) {
    PhaseBoxState s;
    EXPECT_NEAR(simpot::norm(s, 0, P_BPM), (120.0f - 40.0f) / 260.0f, 0.001f);        // 120 BPM
    EXPECT_NEAR(simpot::norm(s, 0, P_WAVE), 0.0f, 0.001f);                             // sine at the left stop
    EXPECT_NEAR(simpot::norm(s, 0, P_MAC_DIV), 4.0f / 9.0f, 0.001f);                    // x1 on the ladder
    EXPECT_NEAR(simpot::norm(s, 1, P_MAC_DIV), 5.0f / 9.0f, 0.001f);                    // Dest 2 is x1.5
    s.macros.destinations[0].clock_mult = 1.3f;                                         // off-ladder preset values show the nearest step
    EXPECT_NEAR(simpot::norm(s, 0, P_MAC_DIV), 5.0f / 9.0f, 0.001f);
}

TEST(SimPot, ClicksStopAtTheEndsInsteadOfWrappingRound) {
    PhaseBoxState s; int d = 0;
    s.osc.shape = PhaseOscillator::TRIANGLE;
    simpot::turn_clamped(s, d, P_WAVE, +1);
    EXPECT_EQ(s.osc.shape, PhaseOscillator::TRIANGLE);          // the encoder would wrap to sine; a pot stops
    s.osc.shape = PhaseOscillator::SINE;
    simpot::turn_clamped(s, d, P_WAVE, -1);
    EXPECT_EQ(s.osc.shape, PhaseOscillator::SINE);
    s.osc.phase_offset = 0.0f;
    simpot::turn_clamped(s, d, P_PHASE, -1);
    EXPECT_NEAR(s.osc.phase_offset, 0.0f, 0.001f);              // phase would wrap to 6.08
    for (int i = 0; i < 40; ++i) simpot::turn_clamped(s, d, P_PHASE, +1);
    EXPECT_LT(s.osc.phase_offset, 6.29f);
    EXPECT_GT(s.osc.phase_offset, 5.0f);                        // it climbed to the top and stayed there
}

TEST(SimPot, NormalClicksStillWorkInTheMiddleOfTheRange) {
    PhaseBoxState s; int d = 0;
    simpot::turn_clamped(s, d, P_CC, +1);   EXPECT_EQ(s.macros.destinations[0].cc_number, 17);
    simpot::turn_clamped(s, d, P_CC, -1);   EXPECT_EQ(s.macros.destinations[0].cc_number, 16);
    simpot::turn_clamped(s, d, P_BPM, +1, false);   EXPECT_NEAR(s.osc.bpm, 121.0f, 0.001f);
    simpot::turn_clamped(s, d, P_BPM, +1, true);    EXPECT_NEAR(s.osc.bpm, 126.0f, 0.001f);
    simpot::turn_clamped(s, d, P_WAVE, +1);         EXPECT_EQ(s.osc.shape, PhaseOscillator::SAW);
    for (int i = 0; i < 30; ++i) simpot::turn_clamped(s, d, P_CC, +1);
    EXPECT_EQ(s.macros.destinations[0].cc_number, 46);
}

TEST(SimPot, ASweepAcrossTheWholeKnobCoversTheWholeRange) {
    PhaseBoxState s; int d = 0;
    float p = 0.0f;
    for (int deg = 0; deg < 300; deg += 3) { p = std::clamp(p + 3.0f / simpot::SWEEP_DEGREES, 0.0f, 1.0f); simpot::set_norm(s, d, P_BPM, p); }
    EXPECT_NEAR(s.osc.bpm, 300.0f, 0.001f);                    // 270 degrees from the bottom-left stop reaches the max
    for (int deg = 0; deg < 600; deg += 3) { p = std::clamp(p - 3.0f / simpot::SWEEP_DEGREES, 0.0f, 1.0f); simpot::set_norm(s, d, P_BPM, p); }
    EXPECT_NEAR(s.osc.bpm, 40.0f, 0.001f);                     // and back; turning further does nothing
}

TEST(SimPot, TheOffsetPageIsUsedWhenTheBuildHasOne) {
    if (!simpot::has_offset_page()) return;                      // this build has 11 pages
    PhaseBoxState s; int d = 0;
    simpot::set_norm(s, d, simpot::OFFSET_PAGE, 0);   EXPECT_NEAR(s.macros.destinations[0].offset, -1.0f, 0.001f);
    simpot::set_norm(s, d, simpot::OFFSET_PAGE, 1);   EXPECT_NEAR(s.macros.destinations[0].offset, 1.0f, 0.001f);
    EXPECT_NEAR(simpot::norm(s, 0, simpot::OFFSET_PAGE), 1.0f, 0.001f);
}
