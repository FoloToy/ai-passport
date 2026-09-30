// Minimal GPIO stub for the host tests: only the calls bsp_button.c needs when it
// hands the shared key pad back to a digital input before deep sleep. The test
// that includes the real BSP implementation provides the definitions and records
// the calls, so this header stays a plain interface.
#pragma once

#include "esp_err.h"

typedef int gpio_num_t;

typedef enum {
    GPIO_MODE_INPUT = 0,
} gpio_mode_t;

typedef enum {
    GPIO_PULLUP_ONLY = 0,
    GPIO_PULLDOWN_ONLY,
    GPIO_PULLUP_PULLDOWN,
    GPIO_FLOATING,
} gpio_pull_mode_t;

esp_err_t gpio_set_direction(gpio_num_t pin, gpio_mode_t mode);
esp_err_t gpio_set_pull_mode(gpio_num_t pin, gpio_pull_mode_t pull);
int gpio_get_level(gpio_num_t pin);
