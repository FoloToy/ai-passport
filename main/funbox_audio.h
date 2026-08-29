#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "bsp_button.h"
#include "esp_err.h"

esp_err_t funbox_audio_start(uint8_t volume, bool background_enabled);
void funbox_audio_stop(void);
void funbox_audio_button(bsp_btn_t button);
void funbox_audio_set_volume(uint8_t volume);
void funbox_audio_set_background(bool enabled);
void funbox_audio_reward(void);
