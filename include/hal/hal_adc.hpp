#ifndef PHASEBOX_HAL_ADC_HPP
#define PHASEBOX_HAL_ADC_HPP

#include <cstdint>

namespace phasebox::hal {

class HalAdc {
public:
    HalAdc() = default;

    /**
     * @brief Initialize RP2040 ADC hardware (GPIO 26 / ADC 0).
     */
    void init();

    /**
     * @brief Read raw 12-bit ADC input (0 to 4095).
     * @return 12-bit unsigned integer.
     */
    uint16_t read_raw();

    /**
     * @brief Read EMA-smoothed normalized expression value.
     * @return Float in range [0.0f, 1.0f].
     */
    float read_normalized();

#ifdef HOST_BUILD
    /**
     * @brief Inject simulated raw ADC value for host unit tests.
     */
    void set_simulated_raw(uint16_t value) { simulated_raw_ = value; }
#endif

private:
    float smoothed_val_{0.0f};
    const float alpha_{0.15f}; // Smoothing factor (lower = smoother)

#ifdef HOST_BUILD
    uint16_t simulated_raw_{0};
#endif
};

} // namespace phasebox::hal

#endif // PHASEBOX_HAL_ADC_HPP
