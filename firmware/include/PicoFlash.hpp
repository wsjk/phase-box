#pragma once
// FlashDevice for the RP2040's own program flash: the last FlashPresetStorage::REGION_BYTES (64 KB) of it.
// Erasing / programming stops code running from flash on *both* cores, so each operation goes through
// flash_safe_execute(): Core 1 must have called flash_safe_execute_core_init() first, and both cores pause
// for the duration (about 1 ms per saved preset; about 45 ms for the sector erase every 8th save of a slot).
#include <cstdint>
#include <cstring>
#include "hardware/flash.h"
#include "pico/flash.h"
#include "FlashPresetStorage.hpp"

extern "C" char __flash_binary_end;   // linker: first byte after the firmware image (an XIP address)

class PicoFlash : public FlashDevice {
public:
    static constexpr uint32_t BASE = PICO_FLASH_SIZE_BYTES - FlashPresetStorage::REGION_BYTES;   // offset into flash
    static constexpr uint32_t LOCKOUT_TIMEOUT_MS = 100;
    static_assert(SECTOR_BYTES == FLASH_SECTOR_SIZE && PAGE_BYTES == FLASH_PAGE_SIZE, "flash geometry");

    /// Refuses to touch flash the firmware itself occupies (only possible with a huge image or a small chip).
    bool available() const override {
        return reinterpret_cast<uintptr_t>(&__flash_binary_end) - XIP_BASE <= BASE;
    }

    void read(uint32_t offset, uint8_t* out, size_t n) override {
        std::memcpy(out, reinterpret_cast<const uint8_t*>(XIP_BASE + BASE + offset), n);
    }

    bool erase_sector(uint32_t offset) override {
        Op op{BASE + offset, nullptr, 0};
        return flash_safe_execute(&do_erase, &op, LOCKOUT_TIMEOUT_MS) == PICO_OK;
    }

    bool program(uint32_t offset, const uint8_t* data, size_t n) override {
        Op op{BASE + offset, data, n};   // data must be in RAM (FlashPresetStorage passes its own buffer)
        return flash_safe_execute(&do_program, &op, LOCKOUT_TIMEOUT_MS) == PICO_OK;
    }

private:
    struct Op { uint32_t offset; const uint8_t* data; size_t n; };
    static void do_erase(void* p) { flash_range_erase(static_cast<Op*>(p)->offset, FLASH_SECTOR_SIZE); }
    static void do_program(void* p) { auto* op = static_cast<Op*>(p); flash_range_program(op->offset, op->data, op->n); }
};
