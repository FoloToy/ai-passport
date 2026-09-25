// eggy_sfx.h — synthesized party sound cues (async RTTTL via rtttl_player).
#pragma once

#include "esp_err.h"

typedef enum {
    EGGY_SFX_SELECT = 0,   // menu move
    EGGY_SFX_BACK,         // return to hub
    EGGY_SFX_START,        // enter a game
    EGGY_SFX_JUMP,
    EGGY_SFX_DUCK,
    EGGY_SFX_PASSED,       // cleared an obstacle
    EGGY_SFX_CRASH,
    EGGY_SFX_GO,           // quick-tap green light
    EGGY_SFX_SPIN_TICK,   // wheel ticking past a segment
    EGGY_SFX_WIN,
    EGGY_SFX_LOSE,
} eggy_sfx_t;

// Boot the audio worker. Call once from app_main after bsp_audio_init.
esp_err_t eggy_sfx_init(void);

// Play a cue; non-blocking, replaces any currently playing cue.
void eggy_sfx_play(eggy_sfx_t cue);
