#include "screensaver.hpp"
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

#ifndef OLED_I2C_PORT
#define OLED_I2C_PORT i2c0
#endif

#ifndef OLED_SDA_PIN
#define OLED_SDA_PIN 4
#endif

#ifndef OLED_SCL_PIN
#define OLED_SCL_PIN 5
#endif

static volatile bool g_user_activity_flag = false;
static bool g_is_dimmed = false;

void screensaver_notify_activity(void) {
    g_user_activity_flag = true;
}

bool screensaver_is_dimmed(void) {
    return g_is_dimmed;
}

void oled_set_contrast(i2c_inst_t* i2c, uint8_t contrast_val) {
    uint8_t cmd[2] = { 0x00, OLED_CMD_SET_CONTRAST };
    i2c_write_blocking(i2c, OLED_I2C_ADDR, cmd, 2, false);
    
    uint8_t val[2] = { 0x00, contrast_val };
    i2c_write_blocking(i2c, OLED_I2C_ADDR, val, 2, false);
}

void core1_ui_loop(void) {
    // Input pins for rotary encoder (A: GP2, B: GP3, SW: GP6) and footswitches (TAP: GP10, MUT: GP11)
    const uint input_pins[] = {2, 3, 6, 10, 11};
    for (uint pin : input_pins) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        gpio_pull_up(pin);
    }

    // Configure I2C on RP2040 for OLED display
    i2c_init(OLED_I2C_PORT, 400 * 1000);
    gpio_set_function(OLED_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(OLED_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(OLED_SDA_PIN);
    gpio_pull_up(OLED_SCL_PIN);

    absolute_time_t last_activity = get_absolute_time();
    g_is_dimmed = false;

    // Track baseline state of input pins
    uint32_t last_gpio_state = 0;
    for (uint pin : input_pins) {
        if (gpio_get(pin)) last_gpio_state |= (1u << pin);
    }

    while (true) {
        // Read current state of input pins
        uint32_t cur_gpio_state = 0;
        for (uint pin : input_pins) {
            if (gpio_get(pin)) cur_gpio_state |= (1u << pin);
        }

        bool user_input_detected = (cur_gpio_state != last_gpio_state);
        last_gpio_state = cur_gpio_state;

        if (g_user_activity_flag) {
            user_input_detected = true;
            g_user_activity_flag = false;
        }

        if (user_input_detected) {
            last_activity = get_absolute_time();
            if (g_is_dimmed) {
                oled_set_contrast(OLED_I2C_PORT, OLED_CONTRAST_NORMAL); // Restore brightness (0xCF)
                g_is_dimmed = false;
            }
        }

        // Check for 30-second timeout (30,000,000 microseconds)
        if (!g_is_dimmed && absolute_time_diff_us(last_activity, get_absolute_time()) > SCREENSAVER_TIMEOUT_US) {
            oled_set_contrast(OLED_I2C_PORT, OLED_CONTRAST_DIM); // Dim display (0x01)
            g_is_dimmed = true;
        }

        sleep_ms(16); // ~60 FPS update loop
    }
}
