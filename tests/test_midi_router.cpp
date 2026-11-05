// Where the simulator's MIDI goes: straight to the named hardware destination, and nowhere else.
#include <gtest/gtest.h>
#include <algorithm>
#include <string>
#include <vector>
#include "MidiRouter.hpp"

using namespace midi_route;

namespace {
struct Fake : Backend {
    std::vector<Endpoint> list;
    struct Sent { uint64_t handle; std::vector<uint8_t> bytes; };
    std::vector<Sent> sent;
    std::vector<std::vector<uint8_t>> virtual_sent;
    int list_calls = 0, virtual_created = 0;
    bool send_fails = false, virtual_ok = true;

    std::vector<Endpoint> destinations() override { ++list_calls; return list; }
    bool send(uint64_t h, const uint8_t* b, size_t n) override {
        if (send_fails) return false;
        // like CoreMIDI: sending to an endpoint that no longer exists fails
        if (std::none_of(list.begin(), list.end(), [&](const Endpoint& e) { return e.handle == h && !e.offline; })) return false;
        sent.push_back({h, std::vector<uint8_t>(b, b + n)});
        return true;
    }
    bool create_virtual_source(const std::string&) override { ++virtual_created; return virtual_ok; }
    bool send_virtual(const uint8_t* b, size_t n) override { virtual_sent.emplace_back(b, b + n); return true; }

    void add(uint64_t h, std::vector<std::string> names, bool offline = false) { list.push_back({h, std::move(names), offline}); }
    void remove(uint64_t h) { list.erase(std::remove_if(list.begin(), list.end(), [&](const Endpoint& e) { return e.handle == h; }), list.end()); }
    size_t sent_to(uint64_t h) const { return std::count_if(sent.begin(), sent.end(), [&](const Sent& s) { return s.handle == h; }); }
};
bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }
} // namespace

TEST(MidiNames, MatchingIgnoresCaseSpacesAndPunctuation) {
    EXPECT_TRUE(name_matches("FRASIL FM-1", "FRASIL FM-1"));
    EXPECT_TRUE(name_matches("melodee fm 1 MIDI 1", "FRASIL FM-1"));
    EXPECT_TRUE(name_matches("MelodeeFM-1", "melodee fm-1"));
    EXPECT_FALSE(name_matches("IAC Driver Bus 1", "FRASIL FM-1"));
    EXPECT_FALSE(name_matches("Melodee FM-2", "FRASIL FM-1"));
    EXPECT_FALSE(name_matches("anything", ""));
}

TEST(MidiConfig, EnvironmentVariableChoosesTheTarget) {
    EXPECT_TRUE(Config::from_env(nullptr).wanted == "FRASIL FM-1");
    EXPECT_TRUE(Config::from_env("").wanted == "FRASIL FM-1");
    EXPECT_TRUE(Config::from_env("   ").wanted == "FRASIL FM-1");
    EXPECT_TRUE(Config::from_env("  Other Synth ").wanted == "Other Synth");
    EXPECT_TRUE(Config::from_env("Virtual").use_virtual);
    EXPECT_FALSE(Config::from_env("Other Synth").use_virtual);
}

TEST(MidiRouter, SendsStraightToTheHardwareDestinationAndNothingElse) {
    Fake f; f.add(10, {"IAC Driver Bus 1"}); f.add(11, {"FRASIL FM-1", "FM-1"}); f.add(12, {"Other Synth"});
    Router r(f, Config{});
    r.poll(0);
    ASSERT_TRUE(r.connected());
    EXPECT_TRUE(r.status() == "-> FRASIL FM-1");
    EXPECT_TRUE(r.send_cc(1, 74, 100));
    EXPECT_TRUE(r.send_cc(4, 71, 5));
    EXPECT_EQ(f.sent_to(11), 2u);
    EXPECT_EQ(f.sent_to(10) + f.sent_to(12), 0u);              // the other devices never receive anything
    EXPECT_EQ(f.sent[0].bytes[0], 0xB0); EXPECT_EQ(f.sent[0].bytes[1], 74); EXPECT_EQ(f.sent[0].bytes[2], 100);
    EXPECT_EQ(f.sent[1].bytes[0], 0xB3);                       // channel 4
    EXPECT_EQ(f.virtual_created, 0);                           // no virtual port is created in this mode
}

TEST(MidiRouter, MessageBytesAreEncodedAsMidi) {
    Fake f; f.add(1, {"FRASIL FM-1"});
    Router r(f, Config{}); r.poll(0);
    r.send_cc(16, 127, 127); r.send_cc(1, 200, 255);           // out-of-range values are masked to 7 bits
    EXPECT_EQ(f.sent[0].bytes[0], 0xBF);
    EXPECT_EQ(f.sent[1].bytes[1], 200 & 0x7F); EXPECT_EQ(f.sent[1].bytes[2], 255 & 0x7F);
}

TEST(MidiRouter, NeverSendsWhenTheDeviceIsNotThereAndExplainsWhy) {
    Fake f; f.add(10, {"IAC Driver Bus 1"}); f.add(12, {"Other Synth"});
    Router r(f, Config{});
    r.poll(0);
    EXPECT_FALSE(r.connected());
    EXPECT_FALSE(r.send_cc(1, 74, 64));
    EXPECT_EQ(f.sent.size(), 0u);                              // not even to the "closest" device
    EXPECT_TRUE(has(r.status(), "no destination matches 'FRASIL FM-1'"));
    EXPECT_TRUE(has(r.status(), "IAC Driver Bus 1") && has(r.status(), "Other Synth"));
}

TEST(MidiRouter, ZeroDestinationsGetsASpecificMessage) {
    Fake f;
    Router r(f, Config{}); r.poll(0);
    EXPECT_TRUE(has(r.status(), "no MIDI destinations at all"));
    EXPECT_TRUE(has(r.status(), "reset_coremidi"));
}

TEST(MidiRouter, AnOfflineMatchIsNotUsedUntilItComesBack) {
    Fake f; f.add(11, {"FRASIL FM-1"}, /*offline=*/true);
    Router r(f, Config{}); r.poll(0);
    EXPECT_FALSE(r.connected());
    EXPECT_TRUE(has(r.status(), "listed but offline"));
    f.list[0].offline = false;
    r.poll(1000);
    EXPECT_TRUE(r.connected());
}

TEST(MidiRouter, FindsADevicePluggedInAfterStartButOnlyScansOncePerSecond) {
    Fake f;
    Router r(f, Config{});
    r.poll(0);
    const int after_first = f.list_calls;
    for (uint32_t t = 16; t < 1000; t += 16) r.poll(t);
    EXPECT_EQ(f.list_calls, after_first);                      // no scanning every frame
    f.add(11, {"FRASIL FM-1"});
    r.poll(1000);
    EXPECT_TRUE(r.connected());
}

TEST(MidiRouter, EpochChangesOnEveryNewConnectionSoValuesCanBeResent) {
    Fake f; f.add(11, {"FRASIL FM-1"});
    Router r(f, Config{});
    const uint32_t before = r.epoch();
    r.poll(0);
    EXPECT_EQ(r.epoch(), before + 1);
    r.poll(500);
    EXPECT_EQ(r.epoch(), before + 1);                          // stays the same while connected
}

TEST(MidiRouter, UnplugDetectedOnSendThenReconnectsToTheNewEndpoint) {
    Fake f; f.add(11, {"FRASIL FM-1"});
    Router r(f, Config{}); r.poll(0);
    ASSERT_TRUE(r.send_cc(1, 74, 1));
    f.remove(11);                                              // USB cable pulled
    EXPECT_FALSE(r.send_cc(1, 74, 2));                         // fails, and the router lets go
    EXPECT_FALSE(r.connected());
    EXPECT_TRUE(has(r.status(), "unplugged"));
    EXPECT_FALSE(r.send_cc(1, 74, 3));                         // nothing is sent while it is away
    r.poll(100);                                               // looks again at once, finds nothing
    f.add(21, {"FRASIL FM-1"});                               // plugged back in: a NEW handle
    const uint32_t e = r.epoch();
    r.poll(1100);
    EXPECT_TRUE(r.connected());
    EXPECT_GT(r.epoch(), e);
    EXPECT_TRUE(r.send_cc(1, 74, 4));
    EXPECT_EQ(f.sent_to(21), 1u);
    EXPECT_EQ(f.sent_to(11), 1u);                              // only the one message from before the unplug
}

TEST(MidiRouter, UnplugDetectedByTheVerifyCheckEvenWithoutSending) {
    Fake f; f.add(11, {"FRASIL FM-1"});
    Router r(f, Config{}); r.poll(0);
    f.remove(11);
    r.poll(1999);
    EXPECT_TRUE(r.connected());                                // not checked yet
    r.poll(2000);
    EXPECT_FALSE(r.connected());
    EXPECT_TRUE(has(r.status(), "went away"));
}

TEST(MidiRouter, ADeviceThatGoesOfflineIsTreatedAsGone) {
    Fake f; f.add(11, {"FRASIL FM-1"});
    Router r(f, Config{}); r.poll(0);
    f.list[0].offline = true;
    r.poll(2000);
    EXPECT_FALSE(r.connected());
}

TEST(MidiRouter, ReorderingTheDeviceListNeverRedirectsMessages) {
    Fake f; f.add(10, {"IAC Driver Bus 1"}); f.add(11, {"FRASIL FM-1"}); f.add(12, {"Other Synth"});
    Router r(f, Config{}); r.poll(0);
    f.remove(10);                                              // another device disappears: list positions all shift
    r.poll(2000);
    EXPECT_TRUE(r.connected());
    r.send_cc(1, 74, 9);
    EXPECT_EQ(f.sent_to(11), 1u);
    EXPECT_EQ(f.sent_to(12), 0u);                              // a position-based router would have hit this one
}

TEST(MidiRouter, TheFirstOfSeveralMatchesIsUsed) {
    Fake f; f.add(11, {"FRASIL FM-1 Port 1"}); f.add(12, {"FRASIL FM-1 Port 2"});
    Router r(f, Config{}); r.poll(0);
    r.send_cc(1, 1, 1);
    EXPECT_EQ(f.sent_to(11), 1u); EXPECT_EQ(f.sent_to(12), 0u);
}

TEST(MidiRouter, OnlyChangesAreLoggedNotEveryScan) {
    Fake f; f.add(10, {"Other Synth"});
    Router r(f, Config{});
    for (uint32_t t = 0; t < 10000; t += 1000) r.poll(t);      // ten scans, nothing found
    auto log = r.take_log();
    int not_found = 0; for (auto& l : log) not_found += has(l, "no destination matches");
    EXPECT_EQ(not_found, 1);
    f.add(11, {"FRASIL FM-1"}); r.poll(10000);
    log = r.take_log();
    ASSERT_EQ(log.size(), 1u);
    EXPECT_TRUE(has(log[0], "FRASIL FM-1"));
    EXPECT_EQ(r.take_log().size(), 0u);                        // taking the log empties it
}

TEST(MidiRouter, DifferentTargetNameFromTheEnvironment) {
    Fake f; f.add(10, {"FRASIL FM-1"}); f.add(12, {"Other Synth"});
    Router r(f, Config::from_env("other synth")); r.poll(0);
    r.send_cc(2, 10, 10);
    EXPECT_EQ(f.sent_to(12), 1u); EXPECT_EQ(f.sent_to(10), 0u);
}

TEST(MidiRouter, VirtualModeCreatesTheSourceOnceAndNeverTouchesHardware) {
    Fake f; f.add(11, {"FRASIL FM-1"});
    Router r(f, Config::from_env("virtual"));
    for (uint32_t t = 0; t < 5000; t += 16) r.poll(t);
    EXPECT_EQ(f.virtual_created, 1);
    EXPECT_EQ(f.list_calls, 0);
    EXPECT_TRUE(r.send_cc(1, 74, 50));
    EXPECT_EQ(f.virtual_sent.size(), 1u);
    EXPECT_EQ(f.sent.size(), 0u);
    EXPECT_TRUE(has(r.status(), "Phase Box Virtual Out"));
}

TEST(MidiRouter, VirtualSourceFailureIsReportedAndRetriedLater) {
    Fake f; f.virtual_ok = false;
    Router r(f, Config::from_env("virtual"));
    r.poll(0);
    EXPECT_FALSE(r.connected());
    EXPECT_TRUE(has(r.status(), "could not create"));
    r.poll(500); EXPECT_EQ(f.virtual_created, 1);              // not hammering
    f.virtual_ok = true; r.poll(1000);
    EXPECT_EQ(f.virtual_created, 2);
    EXPECT_TRUE(r.connected());
}
