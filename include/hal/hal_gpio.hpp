#pragma once
#include <cstdint>

namespace phasebox::hal {
enum class ButtonId {
    FootswitchTapMode,
    FootswitchMutation,
    EncoderSwitch
};

class HalGpio {
public:
    virtual ~HalGpio() = default;
    virtual bool read_button(ButtonId id) = 0;
    virtual int32_t read_encoder_delta() = 0;
};
}
