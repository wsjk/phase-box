#pragma once
#include "hal/hal_flash.hpp"
#include <vector>
#include <map>
#include <cstring>

namespace phasebox::hal {

class MockHalFlash : public HalFlash {
public:
    static constexpr size_t MAX_SLOTS = 16;
    static constexpr size_t MAX_PRESET_SIZE = 512;

    bool save_preset(uint8_t slot, const uint8_t* data, size_t size) override {
        if (slot >= MAX_SLOTS || data == nullptr || size > MAX_PRESET_SIZE) {
            return false;
        }
        slots_[slot] = std::vector<uint8_t>(data, data + size);
        save_count_++;
        last_saved_slot_ = slot;
        return true;
    }

    bool load_preset(uint8_t slot, uint8_t* data, size_t size) override {
        if (slot >= MAX_SLOTS || data == nullptr) {
            return false;
        }
        auto it = slots_.find(slot);
        if (it == slots_.end() || it->second.size() != size) {
            return false;
        }
        std::memcpy(data, it->second.data(), size);
        load_count_++;
        last_loaded_slot_ = slot;
        return true;
    }

    bool has_preset(uint8_t slot) const {
        return slots_.find(slot) != slots_.end();
    }

    uint32_t get_save_count() const { return save_count_; }
    uint32_t get_load_count() const { return load_count_; }
    uint8_t get_last_saved_slot() const { return last_saved_slot_; }
    uint8_t get_last_loaded_slot() const { return last_loaded_slot_; }

    void clear() {
        slots_.clear();
        save_count_ = 0;
        load_count_ = 0;
    }

private:
    std::map<uint8_t, std::vector<uint8_t>> slots_;
    uint32_t save_count_{0};
    uint32_t load_count_{0};
    uint8_t last_saved_slot_{0xFF};
    uint8_t last_loaded_slot_{0xFF};
};

} // namespace phasebox::hal
