#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TMZ_SOUND_MOVE = 0,
    TMZ_SOUND_SELECT,
    TMZ_SOUND_FEED,
    TMZ_SOUND_CLEAN,
    TMZ_SOUND_COIN,
    TMZ_SOUND_SUCCESS,
    TMZ_SOUND_FAIL,
    TMZ_SOUND_GROW,
    TMZ_SOUND_FUSION,
    TMZ_SOUND_CALL,
} tmz_sound_t;

bool tmz_audio_start(uint8_t sound_level);
void tmz_audio_set_level(uint8_t sound_level);
void tmz_audio_play(tmz_sound_t sound, uint8_t motif);
