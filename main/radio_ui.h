#pragma once

#include "esp_err.h"
#include "radio_nvs.h"
#include "radio_player.h"
#include "radio_wifi.h"

#include "bsp_button.h"

esp_err_t radio_ui_start(radio_settings_t *settings);
void radio_ui_on_key(bsp_btn_t btn, bsp_btn_ev_t ev);
void radio_ui_on_wifi(radio_wifi_state_t state, const char *detail);
void radio_ui_on_player(radio_player_state_t state, const char *message);
