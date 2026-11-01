#pragma once
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <cmath>
#include "PhaseBoxState.hpp"

// Preset file format (little-endian, fixed-width so files are portable between
// the RP2040 and the desktop simulator):
//   u32 magic "PBX1" | u16 version | payload | u32 CRC-32 of everything before it
// (version 3; version 2 files from earlier builds are rejected and defaults are used)
// Loading validates magic, version, length, CRC and every field's range, and only
// then commits to the live state (a bad file never half-applies).
class PresetManager {
public:
    // Directory prefix for preset files ("/" on the device filesystem; tests override).
    static inline const char* path_prefix = "/";

    static constexpr uint32_t MAGIC = 0x31584250u; // 'P','B','X','1'
    static constexpr uint16_t FORMAT_VERSION = 3; // v3 adds turing_mix
    static constexpr uint32_t MAX_MACROS = 16;

    static bool savePreset(int slot, const PhaseBoxState& state) {
        if (slot < 1 || slot > 99) return false;
        static uint8_t buf[512];
        size_t n = 0;
        put(buf, n, MAGIC);
        put(buf, n, FORMAT_VERSION);
        put(buf, n, static_cast<int32_t>(state.osc.shape));
        put(buf, n, state.osc.bpm);
        put(buf, n, state.osc.phase_offset);
        put(buf, n, state.osc.pm_depth);
        put(buf, n, static_cast<int32_t>(state.midi_channel));
        put(buf, n, static_cast<int32_t>(state.midi_cc_num));
        put(buf, n, state.turing.mutation_prob);
        put(buf, n, state.turing_mix);

        const size_t count = state.macros.destinations.size();
        if (count > MAX_MACROS) return false;
        put(buf, n, static_cast<uint32_t>(count));
        for (const auto& d : state.macros.destinations) {
            put(buf, n, static_cast<int32_t>(d.cc_number));
            put(buf, n, static_cast<int32_t>(d.midi_channel));
            put(buf, n, d.scale);
            put(buf, n, d.offset);
            put(buf, n, d.clock_mult);
        }
        put(buf, n, crc32(buf, n));

        FILE* f = fopen(filename(slot).c_str(), "wb");
        if (!f) return false;
        bool ok = fwrite(buf, 1, n, f) == n;
        ok = (fclose(f) == 0) && ok;
        return ok;
    }

    static bool loadPreset(int slot, PhaseBoxState& state) {
        if (slot < 1 || slot > 99) return false;
        static uint8_t buf[512];
        FILE* f = fopen(filename(slot).c_str(), "rb");
        if (!f) return false;
        size_t n = fread(buf, 1, sizeof(buf), f);
        bool too_big = !feof(f); // file longer than our buffer
        fclose(f);
        if (too_big || n < 10) return false;

        uint32_t stored_crc;
        std::memcpy(&stored_crc, buf + n - 4, 4);
        if (crc32(buf, n - 4) != stored_crc) return false;

        size_t pos = 0;
        const size_t end = n - 4;
        uint32_t magic; uint16_t version;
        if (!get(buf, pos, end, magic) || magic != MAGIC) return false;
        if (!get(buf, pos, end, version) || version != FORMAT_VERSION) return false;

        int32_t shape, channel, cc; float bpm, phase_offset, pm_depth, mutation, mix; uint32_t count;
        if (!get(buf, pos, end, shape) || !get(buf, pos, end, bpm) ||
            !get(buf, pos, end, phase_offset) || !get(buf, pos, end, pm_depth) ||
            !get(buf, pos, end, channel) || !get(buf, pos, end, cc) ||
            !get(buf, pos, end, mutation) || !get(buf, pos, end, mix) ||
            !get(buf, pos, end, count)) return false;

        if (shape < 0 || shape > 3) return false;
        if (!std::isfinite(bpm) || bpm < 20.0f || bpm > 300.0f) return false;
        if (!std::isfinite(phase_offset) || !std::isfinite(pm_depth)) return false;
        if (channel < 1 || channel > 16 || cc < 0 || cc > 127) return false;
        if (!std::isfinite(mutation) || mutation < 0.0f || mutation > 1.0f) return false;
        if (!std::isfinite(mix) || mix < 0.0f || mix > 1.0f) return false;
        if (count > MAX_MACROS || count > state.macros.destinations.size()) return false;

        MacroDestination parsed[MAX_MACROS];
        for (uint32_t i = 0; i < count; ++i) {
            int32_t dcc, dch; float scale, offset, mult;
            if (!get(buf, pos, end, dcc) || !get(buf, pos, end, dch) ||
                !get(buf, pos, end, scale) || !get(buf, pos, end, offset) ||
                !get(buf, pos, end, mult)) return false;
            if (dcc < 0 || dcc > 127 || dch < 1 || dch > 16) return false;
            if (!std::isfinite(scale) || scale < -2.0f || scale > 2.0f) return false;
            if (!std::isfinite(offset) || offset < -1.0f || offset > 1.0f) return false;
            if (!std::isfinite(mult) || mult < 0.125f || mult > 8.0f) return false;
            parsed[i] = {dcc, dch, scale, offset, mult};
        }

        // Everything validated: commit.
        state.osc.shape = static_cast<PhaseOscillator::WaveShape>(shape);
        state.osc.bpm = bpm;
        state.osc.phase_offset = phase_offset;
        state.osc.pm_depth = pm_depth;
        state.midi_channel = channel;
        state.midi_cc_num = cc;
        state.turing.mutation_prob = mutation;
        state.turing_mix = mix;
        for (uint32_t i = 0; i < count; ++i) state.macros.destinations[i] = parsed[i];
        state.macros.reset_phases();
        return true;
    }

private:
    static std::string filename(int slot) {
        char tail[32];
        snprintf(tail, sizeof(tail), "preset_%d.bin", slot);
        return std::string(path_prefix) + tail;
    }

    template <typename T>
    static void put(uint8_t* buf, size_t& n, T v) {
        std::memcpy(buf + n, &v, sizeof(T));
        n += sizeof(T);
    }

    template <typename T>
    static bool get(const uint8_t* buf, size_t& pos, size_t end, T& v) {
        if (pos + sizeof(T) > end) return false;
        std::memcpy(&v, buf + pos, sizeof(T));
        pos += sizeof(T);
        return true;
    }

    static uint32_t crc32(const uint8_t* data, size_t len) {
        uint32_t crc = 0xFFFFFFFFu;
        for (size_t i = 0; i < len; ++i) {
            crc ^= data[i];
            for (int b = 0; b < 8; ++b) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
        return ~crc;
    }
};
