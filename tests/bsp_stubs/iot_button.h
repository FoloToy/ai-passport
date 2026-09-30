#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "sdkconfig.h"
typedef struct button_driver_t button_driver_t;
struct button_driver_t {
    bool enable_power_save;
    uint8_t (*get_key_level)(button_driver_t *);
    esp_err_t (*enter_power_save)(button_driver_t *);
    esp_err_t (*exit_power_save)(button_driver_t *);
    int32_t (*get_gpio_num)(button_driver_t *);
    esp_err_t (*del)(button_driver_t *);
};
enum { BUTTON_INACTIVE, BUTTON_ACTIVE };
typedef struct button_dev_t *button_handle_t;
typedef struct { uint16_t long_press_time, short_press_time; } button_config_t;
typedef enum { BUTTON_PRESS_DOWN, BUTTON_PRESS_UP, BUTTON_SINGLE_CLICK, BUTTON_DOUBLE_CLICK, BUTTON_LONG_PRESS_START } button_event_t;
typedef struct { int unused; } button_event_args_t;
typedef void (*button_cb_t)(void *, void *);
esp_err_t iot_button_create(const button_config_t *, const button_driver_t *, button_handle_t *);
esp_err_t iot_button_delete(button_handle_t);
esp_err_t iot_button_register_cb(button_handle_t, button_event_t, button_event_args_t *, button_cb_t, void *);
