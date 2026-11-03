// Building blocks of the device firmware: encoder decoding, debouncing, tap tempo, MIDI throttling,
// screensaver, frame buffer, and the lock-free frame hand-off between the two cores.
#include <gtest/gtest.h>
#include <atomic>
#include <chrono>
#include <cstring>
#include <thread>
#include <vector>
#include "MidiChangeFilter.hpp"
#include "ScreenSaver.hpp"
#include "TapTempo.hpp"
#include "hal/debounce.hpp"
#include "hal/frame_buffer.hpp"
#include "hal/quadrature.hpp"
#include "hal/seqlock_buffer.hpp"

using namespace phasebox::hal;

namespace {
struct AB { bool a, b; };
// One detent clockwise from rest (A = B = 1): A leads B.
const AB CW[4]  = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
const AB CCW[4] = {{1, 0}, {0, 0}, {0, 1}, {1, 1}};
int run(QuadratureDecoder& q, const AB* seq, int n = 4) { int total = 0; for (int i = 0; i < n; ++i) total += q.feed(seq[i].a, seq[i].b); return total; }
} // namespace

TEST(Quadrature, OneStepPerDetentNotFour) {
    QuadratureDecoder q; q.feed(true, true);
    EXPECT_EQ(run(q, CW), +1);
    EXPECT_EQ(run(q, CW), +1);
    EXPECT_EQ(run(q, CCW), -1);
}

TEST(Quadrature, ThreeDetentsGiveThreeSteps) {
    QuadratureDecoder q; q.feed(true, true);
    int total = 0;
    for (int i = 0; i < 3; ++i) total += run(q, CW);
    EXPECT_EQ(total, 3);
}

TEST(Quadrature, ContactBounceAroundRestGivesNothing) {
    QuadratureDecoder q; q.feed(true, true);
    int total = 0;
    for (int i = 0; i < 20; ++i) { total += q.feed(false, true); total += q.feed(true, true); }
    EXPECT_EQ(total, 0);
}

TEST(Quadrature, TurningBackBeforeTheDetentCancels) {
    QuadratureDecoder q; q.feed(true, true);
    int total = q.feed(false, true) + q.feed(false, false) + q.feed(false, true) + q.feed(true, true);
    EXPECT_EQ(total, 0);
}

TEST(Quadrature, MissedEdgeIsIgnoredAndTheNextDetentIsStillClean) {
    QuadratureDecoder q; q.feed(true, true);
    int total = q.feed(false, true);       // 11 -> 01
    total += q.feed(true, false);          // 01 -> 10: both pins changed, an edge was missed: ignored
    total += q.feed(true, true);           // back at rest: the half-finished turn is dropped
    EXPECT_EQ(total, 0);
    EXPECT_EQ(run(q, CW), +1);             // and the next real detent works normally
}

TEST(Quadrature, ReversedFlipsDirection) {
    QuadratureDecoder q(4, true); q.feed(true, true);
    EXPECT_EQ(run(q, CW), -1);
}

TEST(Quadrature, TwoTransitionEncoders) {
    QuadratureDecoder q(2); q.feed(true, true);
    EXPECT_EQ(q.feed(false, true) + q.feed(false, false), +1);
}

TEST(Debounce, ShortGlitchesAreIgnoredAndRealPressesAreSeenOnce) {
    Debounce d(20);
    d.update(true, 0); d.update(false, 5); d.update(true, 8); d.update(false, 12);   // bouncing
    EXPECT_FALSE(d.state());
    int presses = 0;
    for (uint32_t t = 100; t < 200; ++t) { d.update(true, t); presses += d.rose(); }
    EXPECT_TRUE(d.state());
    EXPECT_EQ(presses, 1);
    int releases = 0;
    for (uint32_t t = 200; t < 300; ++t) { d.update(false, t); releases += d.fell(); }
    EXPECT_FALSE(d.state());
    EXPECT_EQ(releases, 1);
}

TEST(Debounce, WorksAcrossTheClockWrapping) {
    Debounce d(20);
    d.update(true, 0xFFFFFFF0u);
    EXPECT_FALSE(d.state());
    d.update(true, 0x0000000Au);          // 26 ms later, after the wrap
    EXPECT_TRUE(d.state());
}

TEST(TapTempoTest, NeedsTwoTapsThenAveragesAndRestartsAfterAPause) {
    TapTempo t; float bpm = 0;
    EXPECT_FALSE(t.tap(1000, bpm));
    EXPECT_TRUE(t.tap(1500, bpm));  EXPECT_NEAR(bpm, 120.0f, 0.01f);
    EXPECT_TRUE(t.tap(2100, bpm));  EXPECT_NEAR(bpm, 60000.0f / 550.0f, 0.01f);   // average of 500 and 600
    EXPECT_FALSE(t.tap(9000, bpm));                                                // long pause: start over
    EXPECT_TRUE(t.tap(9400, bpm));  EXPECT_NEAR(bpm, 150.0f, 0.01f);
}

TEST(TapTempoTest, ClampsToTheUsableRangeAndKeepsOnlyTheLastFourTaps) {
    TapTempo t; float bpm = 0;
    t.tap(0, bpm); EXPECT_TRUE(t.tap(100, bpm)); EXPECT_NEAR(bpm, 300.0f, 0.01f);   // 600 BPM -> clamped
    TapTempo u;
    u.tap(0, bpm); u.tap(2400, bpm);                     // 25 BPM -> clamped up to 40
    EXPECT_NEAR(bpm, 40.0f, 0.01f);
    TapTempo w;
    uint32_t times[] = {0, 1000, 2000, 3000, 4000, 4500, 5000, 5500};   // slow taps, then 500 ms apart
    for (uint32_t x : times) w.tap(x, bpm);
    EXPECT_NEAR(bpm, 120.0f, 0.01f);                                    // only the last four taps (4000..5500) count
}

TEST(MidiFilter, SendsOnChangeAndRefreshesButNotEveryFrame) {
    MidiChangeFilter f;
    EXPECT_TRUE(f.should_send(0, 1, 16, 64, 0));
    EXPECT_FALSE(f.should_send(0, 1, 16, 64, 17));
    EXPECT_TRUE(f.should_send(0, 1, 16, 65, 34));           // value changed
    EXPECT_TRUE(f.should_send(0, 2, 16, 65, 51));           // channel changed
    EXPECT_TRUE(f.should_send(0, 2, 17, 65, 68));           // CC number changed
    EXPECT_FALSE(f.should_send(0, 2, 17, 65, 900));
    EXPECT_TRUE(f.should_send(0, 2, 17, 65, 1068));         // 1 s refresh
    EXPECT_TRUE(f.should_send(1, 1, 16, 64, 68));           // another destination is independent
    EXPECT_TRUE(f.should_send(99, 1, 16, 64, 68));          // out-of-range index never blocks
}

TEST(ScreenSaverTest, DimsAfterThirtySecondsAndWakesOnActivity) {
    ScreenSaver s;
    EXPECT_FALSE(s.update(29999, 0));
    EXPECT_TRUE(s.update(30000, 0));
    EXPECT_EQ(s.contrast(), ScreenSaver::CONTRAST_DIM);
    EXPECT_FALSE(s.update(40000, 0));                       // already dim: nothing to do
    EXPECT_TRUE(s.update(40001, 40000));                    // activity: back to normal
    EXPECT_EQ(s.contrast(), ScreenSaver::CONTRAST_NORMAL);
}

TEST(FrameBufferTest, PixelsAndTextLandInSsd1306PageLayout) {
    FrameBuffer fb(128, 32);
    fb.draw_pixel(0, 0, true);
    EXPECT_EQ(fb.data()[0], 0x01);
    fb.draw_pixel(5, 9, true);                              // page 1, bit 1
    EXPECT_EQ(fb.data()[5 + 128], 0x02);
    fb.draw_pixel(500, 500, true);                          // off-screen is ignored
    fb.clear();
    fb.draw_string(0, 0, "A");
    int lit = 0; for (size_t i = 0; i < fb.size(); ++i) for (int b = 0; b < 8; ++b) lit += (fb.data()[i] >> b) & 1;
    EXPECT_GE(lit, 10);
    EXPECT_EQ(fb.size(), 512u);
    EXPECT_EQ(FrameBuffer(128, 64).size(), 1024u);
}

TEST(SeqlockBufferTest, ReadersNeverSeeATornFrameWhileAWriterKeepsUpdating) {
    // Like the firmware: one writer publishing frames with idle time in between, other threads reading.
    // (A writer that never pauses would starve any seqlock reader, so this test must not do that.)
    constexpr size_t N = 1024;
    SeqlockBuffer<N> shared;
    std::atomic<bool> stop{false};
    std::thread writer([&] {
        std::vector<uint8_t> frame(N);
        for (uint32_t i = 1; !stop.load(); ++i) {
            std::memset(frame.data(), static_cast<int>(i & 0xFF), N);   // every byte of a frame is the same value
            shared.write(frame.data());
            std::this_thread::sleep_for(std::chrono::microseconds(20));
        }
    });

    std::atomic<long> good{0}, torn{0}, retried{0};
    auto reader = [&] {
        std::vector<uint8_t> out(N);
        const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(300);
        long g = 0, t = 0, r = 0;
        while (std::chrono::steady_clock::now() < until) {
            if (!shared.read(out.data())) { ++r; continue; }
            bool uniform = true;
            for (size_t k = 1; k < N; ++k) uniform = uniform && out[k] == out[0];
            uniform ? ++g : ++t;
        }
        good += g; torn += t; retried += r;
    };
    std::thread r1(reader), r2(reader);
    reader();
    r1.join(); r2.join();
    stop = true;
    writer.join();

    EXPECT_EQ(torn.load(), 0L);          // the property that matters: a reader never gets a mixed frame
    EXPECT_GE(good.load(), 100L);        // and with a paced writer readers do get frames
    (void)retried;
}

TEST(SeqlockBufferTest, SequenceAdvancesByTwoPerWrite) {
    SeqlockBuffer<8> b;
    uint8_t in[8] = {1,2,3,4,5,6,7,8}, out[8]; uint32_t s0, s1;
    EXPECT_TRUE(b.read(out, &s0));
    b.write(in);
    EXPECT_TRUE(b.read(out, &s1));
    EXPECT_EQ(s1, s0 + 2);
    EXPECT_EQ(std::memcmp(in, out, 8), 0);
}
