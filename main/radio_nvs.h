#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

#define RADIO_SSID_MAX 32
#define RADIO_PASS_MAX 64
#define RADIO_STATION_ID_MAX 24

typedef struct {
    char ssid[RADIO_SSID_MAX];
    char password[RADIO_PASS_MAX];
    char station_id[RADIO_STATION_ID_MAX];
    uint8_t volume; /* 0..100 */
    bool has_wifi;
} radio_settings_t;

esp_err_t radio_nvs_load(radio_settings_t *out);
esp_err_t radio_nvs_save(const radio_settings_t *in);
esp_err_t radio_nvs_clear_wifi(void);
