// mini_sfx.c — RTTTL cue strings for mini-world.
#include "mini_sfx.h"
#include "rtttl_player.h"

static const char *const MINI_SONGS[] = {
    [MINI_SFX_STEP]  = "step:d=32,o=5,b=240:16p,c",
    [MINI_SFX_JUMP]  = "jump:d=16,o=6,b=200:c,e",
    [MINI_SFX_DIG]    = "dig:d=32,o=4,b=240:c,16p,c",
    [MINI_SFX_PLACE] = "place:d=16,o=5,b=200:g,c",
    [MINI_SFX_LAND]   = "land:d=32,o=4,b=200:c",
};

esp_err_t mini_sfx_init(void) {
    return rtttl_player_start();
}

void mini_sfx_play(mini_sfx_t cue) {
    if ((unsigned)cue < sizeof(MINI_SONGS) / sizeof(MINI_SONGS[0]) && MINI_SONGS[cue]) {
        (void)rtttl_player_play(MINI_SONGS[cue]);
    }
}
