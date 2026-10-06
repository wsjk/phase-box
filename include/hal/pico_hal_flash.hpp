#pragma once
#include "hal/hal_flash.hpp"
#include <cstdint>
#include <cstring>
#include <algorithm>

#if __has_include("hardware/flash.h") && __has_include("hardware/sync.h")
#include "hardware/flash.h"
#include "hardware/sync.h"
#define PHASEBOX_PICO_FLASH_AVAILABLE 1
#endif

namespace phasebox::hal {

/**
 * @brief RP2040 Hardware SPI NOR Flash Persistence Driver
 * 
 * Reserves the last 64 KB of RP2040 flash (offset 0x1F0000 on 2MB Pico)
 * for storing structured presets.
 */
class PicoHalFlash : public HalFlash {
public:
    static constexpr size_t MAX_SLOTS = 16;
    static constexpr size_t SLOT_SIZE = 256; // 256 bytes per slot
    static constexpr uint32_t FLASH_RESERVED_OFFSET = (2 * 1024 * 1024) - (64 * 1024); // 0x1F0000

#if defined(PHASEBOX_PICO_FLASH_AVAILABLE)
    bool save_preset(uint8_t slot, const uint8_t* data, size_t size) override {
        if (slot >= MAX_SLOTS || data == nullptr || size > SLOT_SIZE) {
            return false;
        }

        // Prepare 4096-byte sector buffer
        uint8_t sector_buffer[FLASH_SECTOR_SIZE];
        const uint8_t* flash_mem = reinterpret_cast<const uint8_t*>(XIP_BASE + FLASH_RESERVED_OFFSET);
        std::memcpy(sector_buffer, flash_mem, FLASH_SECTOR_SIZE);

        // Update target slot in sector buffer
        size_t slot_offset = slot * SLOT_SIZE;
        std::memset(&sector_buffer[slot_offset], 0, SLOT_SIZE);
        std::memcpy(&sector_buffer[slot_offset], data, size);

        // Flash erase and program must be protected with interrupts disabled
        uint32_t ints = save_and_disable_interrupts();
        flash_range_erase(FLASH_RESERVED_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(FLASH_RESERVED_OFFSET, sector_buffer, FLASH_SECTOR_SIZE);
        restore_interrupts(ints);

        return true;
    }

    bool load_preset(uint8_t slot, uint8_t* data, size_t size) override {
        if (slot >= MAX_SLOTS || data == nullptr || size > SLOT_SIZE) {
            return false;
        }

        const uint8_t* flash_mem = reinterpret_cast<const uint8_t*>(XIP_BASE + FLASH_RESERVED_OFFSET);
        size_t slot_offset = slot * SLOT_SIZE;
        std::memcpy(data, flash_mem + slot_offset, size);
        return true;
    }

#else
    // Fallback stub for host simulation build
    bool save_preset(uint8_t, const uint8_t*, size_t) override { return false; }
    bool load_preset(uint8_t, uint8_t*, size_t) override { return false; }
#endif
};

} // namespace phasebox::hal
