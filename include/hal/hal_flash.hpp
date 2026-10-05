#pragma once
#include <cstddef>
#include <cstdint>

namespace phasebox::hal {
class HalFlash {
public:
    virtual ~HalFlash() = default;
    virtual bool save_preset(uint8_t slot, const uint8_t* data, size_t size) = 0;
    virtual bool load_preset(uint8_t slot, uint8_t* data, size_t size) = 0;
};
}
