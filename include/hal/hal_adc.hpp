#pragma once
#include <cstdint>

namespace phasebox::hal {

class HalAdc {
public:
    virtual ~HalAdc() = default;
    /**
     * @brief Read analog value from specified ADC channel.
     * @param channel ADC channel index (0 for GP26 expression pedal).
     * @return Normalized floating-point value in range [0.0f, 1.0f].
     */
    virtual float read_normalized(uint8_t channel) = 0;
};

} // namespace phasebox::hal
