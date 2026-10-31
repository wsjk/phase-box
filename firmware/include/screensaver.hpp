#pragma once
#include <cstdint>
#include <stdbool.h>

#if __has_include("hardware/i2c.h")
#include "hardware/i2c.h"
#endif

#define OLED_I2C_ADDR 0x3C
#define OLED_CMD_SET_CONTRAST 0x81
#define OLED_CONTRAST_NORMAL 0xCF
#define OLED_CONTRAST_DIM 0x01
#define SCREENSAVER_TIMEOUT_US 30000000ULL // 30 seconds of inactivity

#if __has_include("hardware/i2c.h")
void oled_set_contrast(i2c_inst_t* i2c, uint8_t contrast_val);
#endif

void screensaver_notify_activity(void);
bool screensaver_is_dimmed(void);
void core1_ui_loop(void);
