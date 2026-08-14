#pragma once

#include "esp_err.h"

/* Lightweight, asynchronous RTTTL playback for short UI/game effects. */
esp_err_t rtttl_player_start(void);
/* Cancels playback; the worker remains idle so codec locks are never abandoned. */
void rtttl_player_stop(void);
/* Starts immediately and replaces an older queued/playing effect. */
esp_err_t rtttl_player_play(const char *song);
