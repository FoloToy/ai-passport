#pragma once

#include "esp_err.h"
#include "radio_nvs.h"

#include <stdbool.h>

typedef enum {
    RADIO_WIFI_IDLE = 0,
    RADIO_WIFI_CONNECTING,
    RADIO_WIFI_CONNECTED,
    RADIO_WIFI_FAILED,
    RADIO_WIFI_PROVISIONING,
} radio_wifi_state_t;

typedef void (*radio_wifi_cb_t)(radio_wifi_state_t state, const char *detail, void *user);

esp_err_t radio_wifi_init(radio_wifi_cb_t cb, void *user);
esp_err_t radio_wifi_connect_saved(const radio_settings_t *settings);
esp_err_t radio_wifi_start_softap(void);
void radio_wifi_stop_softap(void);
bool radio_wifi_is_connected(void);
radio_wifi_state_t radio_wifi_state(void);
const char *radio_wifi_ip(void);
const char *radio_wifi_softap_ssid(void);
