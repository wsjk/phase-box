---
name: phase-box-presets
description: >-
  Preset persistence, binary serialization, and storage session runbook for Phase Box.
  Use when modifying PhaseBoxState struct fields, changing binary preset formats (PBX1 v3),
  adjusting auto-save (2s idle) or hold-to-save (1s) timers, or adding flash storage backends.
---

# Phase Box Preset Management Skill

This skill provides procedures for maintaining and extending preset serialization, auto-save timers, and storage backends.

---

## 💾 Core Rules for Preset Storage

1. **Safety & Atomicity:**
   - Preset decoding (`PresetManager::decode()`) must be **all-or-nothing**.
   - Magic, format version, payload length, and 32-bit CRC must match before any byte touches `PhaseBoxState`.
   - Every floating-point value is checked with `std::isfinite()` and clamped to its valid domain. A corrupted or partially written file must NEVER partially apply or corrupt live memory.

2. **Little-Endian Fixed-Width Portability:**
   - Presets generated on macOS/Linux simulator must load identically on 32-bit RP2040 Cortex-M0+ silicon and vice versa.
   - All integers and floating-point values are encoded in fixed-width IEEE-754 / two's complement formats.

3. **Dirty-State Auto-Save:**
   - Parameter adjustments trigger an internal 2-second dirty-timer (`AUTO_SAVE_DELAY_MS = 2000`).
   - If no further changes occur within 2 seconds, `PresetSession::update()` silently commits to the active slot.
   - Non-preset state (current menu page, running oscillator phase, pedal position) NEVER marks the session dirty or triggers saves.

---

## 📦 Binary Format Specification (`PBX1` v3)

See [binary_format.md](./references/binary_format.md) for full byte offsets and field definitions.

```
+---------------+---------------+---------------------------------------+---------------+
| Magic (4B)    | Version (2B)  | Parameter Payload (N Bytes)           | CRC-32 (4B)   |
| "PBX1"        | 0x0003        | Shapes, BPM, Phase, Turing, Macros... | IEEE 802.3    |
+---------------+---------------+---------------------------------------+---------------+
```

---

## 🛠️ Step-by-Step Recipes

### Recipe: Adding a New Parameter to Presets

When adding a parameter to `PhaseBoxState`:
1. **Increment `FORMAT_VERSION`** in `firmware/include/PresetManager.hpp`:
   ```cpp
   static constexpr uint16_t FORMAT_VERSION = 4;
   ```
2. **Append field to `PresetManager::encode()`**:
   ```cpp
   put(buf, n, state.my_new_parameter);
   ```
3. **Extract and validate in `PresetManager::decode()`**:
   ```cpp
   float new_val;
   if (!get(buf, pos, end, new_val) || !std::isfinite(new_val) || new_val < MIN || new_val > MAX) {
       return false;
   }
   state.my_new_parameter = new_val;
   ```
4. **Update `tests/test_preset_manager.cpp`** and run tests:
   ```bash
   ./build-test/run_host_tests --gtest_filter="*Preset*"
   ```

---

## 🗄️ Storage Backends

| Backend | Platform | Behavior |
| :--- | :--- | :--- |
| `FilePresetStorage` | Desktop / Tests | Reads/writes `preset_1.bin` through `preset_8.bin` in current directory (or `$PHASE_BOX_PRESET_DIR`). |
| `NoStorage` | RP2040 Firmware | Active default on Pico until flash sector LittleFS backend is linked; shows `NO STORAGE` on hold-to-save. |
| `MemoryPresetStorage`| Unit Tests | In-memory RAM buffer for testing dirty-timers and slot swapping without filesystem I/O. |
