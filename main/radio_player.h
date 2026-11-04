#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    RADIO_PLAYER_IDLE = 0,
    RADIO_PLAYER_CONNECTING,
    RADIO_PLAYER_BUFFERING,
    RADIO_PLAYER_PLAYING,
    RADIO_PLAYER_PAUSED,
    RADIO_PLAYER_ERROR,
} radio_player_state_t;

typedef void (*radio_player_status_cb_t)(radio_player_state_t state,
                                        const char *message,
                                        void *user);

esp_err_t radio_player_init(radio_player_status_cb_t cb, void *user);
void radio_player_set_volume(uint8_t percent);
esp_err_t radio_player_play_url(const char *url);
void radio_player_pause(void);
void radio_player_resume(void);
void radio_player_stop(void);
radio_player_state_t radio_player_state(void);
const char *radio_player_last_error(void);
