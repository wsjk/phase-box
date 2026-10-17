#pragma once
#include <cstdio>
#include <cstring>
#include "PhaseBoxState.hpp"

class PresetManager {
public:
    static bool savePreset(int slot, const PhaseBoxState& state) {
        char filename[32];
        snprintf(filename, sizeof(filename), "/preset_%d.bin", slot);
        
        FILE* f = fopen(filename, "wb");
        if (!f) return false;

        // Serialize core state parameters
        fwrite(&state.osc.shape, sizeof(state.osc.shape), 1, f);
        fwrite(&state.osc.bpm, sizeof(state.osc.bpm), 1, f);
        fwrite(&state.osc.phase_offset, sizeof(state.osc.phase_offset), 1, f);
        fwrite(&state.osc.pm_depth, sizeof(state.osc.pm_depth), 1, f);
        fwrite(&state.midi_channel, sizeof(state.midi_channel), 1, f);
        fwrite(&state.midi_cc_num, sizeof(state.midi_cc_num), 1, f);
        fwrite(&state.turing.mutation_prob, sizeof(state.turing.mutation_prob), 1, f);

        // Serialize macro destinations count and values
        size_t macro_count = state.macros.destinations.size();
        fwrite(&macro_count, sizeof(macro_count), 1, f);
        for (const auto& dest : state.macros.destinations) {
            fwrite(&dest.cc_number, sizeof(dest.cc_number), 1, f);
            fwrite(&dest.midi_channel, sizeof(dest.midi_channel), 1, f);
            fwrite(&dest.scale, sizeof(dest.scale), 1, f);
            fwrite(&dest.offset, sizeof(dest.offset), 1, f);
            fwrite(&dest.clock_mult, sizeof(dest.clock_mult), 1, f);
        }

        fclose(f);
        return true;
    }

    static bool loadPreset(int slot, PhaseBoxState& state) {
        char filename[32];
        snprintf(filename, sizeof(filename), "/preset_%d.bin", slot);
        
        FILE* f = fopen(filename, "rb");
        if (!f) return false;

        fread(&state.osc.shape, sizeof(state.osc.shape), 1, f);
        fread(&state.osc.bpm, sizeof(state.osc.bpm), 1, f);
        fread(&state.osc.phase_offset, sizeof(state.osc.phase_offset), 1, f);
        fread(&state.osc.pm_depth, sizeof(state.osc.pm_depth), 1, f);
        fread(&state.midi_channel, sizeof(state.midi_channel), 1, f);
        fread(&state.midi_cc_num, sizeof(state.midi_cc_num), 1, f);
        fread(&state.turing.mutation_prob, sizeof(state.turing.mutation_prob), 1, f);

        size_t macro_count = 0;
        fread(&macro_count, sizeof(macro_count), 1, f);
        if (macro_count <= state.macros.destinations.size()) {
            for (size_t i = 0; i < macro_count; ++i) {
                fread(&state.macros.destinations[i].cc_number, sizeof(state.macros.destinations[i].cc_number), 1, f);
                fread(&state.macros.destinations[i].midi_channel, sizeof(state.macros.destinations[i].midi_channel), 1, f);
                fread(&state.macros.destinations[i].scale, sizeof(state.macros.destinations[i].scale), 1, f);
                fread(&state.macros.destinations[i].offset, sizeof(state.macros.destinations[i].offset), 1, f);
                fread(&state.macros.destinations[i].clock_mult, sizeof(state.macros.destinations[i].clock_mult), 1, f);
            }
        }

        fclose(f);
        return true;
    }
};
