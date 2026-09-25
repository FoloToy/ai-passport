// Host-test stub for ESP-IDF driver/gpio.h. Only the symbols and types that
// components/bsp/src/bsp_button.c references for deep-sleep are declared here;
// the host test never calls them, so static-inline no-ops are enough.
#pragma once

#include "esp_err.h"
#include <stdint.h>

#define GPIO_MODE_DISABLE     0
#define GPIO_MODE_INPUT       1
#define GPIO_MODE_OUTPUT      2

#define GPIO_PULLUP_DISABLE   0
#define GPIO_PULLUP_ENABLE    1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_PULLDOWN_ENABLE  1

#define GPIO_INTR_DISABLE     0

typedef struct {
    uint64_t pin_bit_mask;
    int mode;
    int pull_up_en;
    int pull_down_en;
    int intr_type;
} gpio_config_t;

static inline esp_err_t gpio_config(const gpio_config_t *cfg) {
    (void)cfg;
    return ESP_OK;
}

static inline int gpio_get_level(int gpio_num) {
    (void)gpio_num;
    return 1;
}
