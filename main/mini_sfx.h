// mini_sfx.h — synthesized mini-world sound cues (async RTTTL).
#pragma once

#include "esp_err.h"

typedef enum {
    MINI_SFX_STEP = 0,
    MINI_SFX_JUMP,
    MINI_SFX_DIG,
    MINI_SFX_PLACE,
    MINI_SFX_LAND,
} mini_sfx_t;

esp_err_t mini_sfx_init(void);
void mini_sfx_play(mini_sfx_t cue);
