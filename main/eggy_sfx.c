// eggy_sfx.c — RTTTL cue strings for the eggy party.
#include "eggy_sfx.h"
#include "rtttl_player.h"

static const char *const EGGY_SONGS[] = {
    [EGGY_SFX_SELECT]    = "sel:d=16,o=6,b=200:c",
    [EGGY_SFX_BACK]      = "back:d=16,o=6,b=120:c,a",
    [EGGY_SFX_START]     = "start:d=8,o=6,b=160:c,e,g",
    [EGGY_SFX_JUMP]      = "jump:d=16,o=6,b=200:c,e",
    [EGGY_SFX_DUCK]      = "duck:d=32,o=4,b=240:c",
    [EGGY_SFX_PASSED]    = "pass:d=32,o=7,b=320:c",
    [EGGY_SFX_CRASH]     = "crash:d=8,o=5,b=100:g,f,e,d,c",
    [EGGY_SFX_GO]        = "go:d=16,o=6,b=220:g",
    [EGGY_SFX_SPIN_TICK] = "tick:d=32,o=7,b=400:c",
    [EGGY_SFX_WIN]       = "win:d=8,o=6,b=160:c,e,g,c7",
    [EGGY_SFX_LOSE]      = "lose:d=8,o=5,b=100:e,d,c",
};

esp_err_t eggy_sfx_init(void) {
    return rtttl_player_start();
}

void eggy_sfx_play(eggy_sfx_t cue) {
    if ((unsigned)cue < sizeof(EGGY_SONGS) / sizeof(EGGY_SONGS[0]) && EGGY_SONGS[cue]) {
        (void)rtttl_player_play(EGGY_SONGS[cue]);
    }
}
