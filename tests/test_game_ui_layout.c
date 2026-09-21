#include <assert.h>

#include "game_ui_layout.h"

static int point_is_inside_rounded_screen(int x, int y)
{
    if (x < 0 || y < 0 ||
        x >= GAME_UI_SCREEN_WIDTH || y >= GAME_UI_SCREEN_HEIGHT) return 0;

    int dx = 0;
    if (x < GAME_UI_SCREEN_RADIUS) {
        dx = GAME_UI_SCREEN_RADIUS - x;
    } else if (x >= GAME_UI_SCREEN_WIDTH - GAME_UI_SCREEN_RADIUS) {
        dx = x - (GAME_UI_SCREEN_WIDTH - 1 - GAME_UI_SCREEN_RADIUS);
    } else {
        return 1;
    }

    int dy = 0;
    if (y < GAME_UI_SCREEN_RADIUS) {
        dy = GAME_UI_SCREEN_RADIUS - y;
    } else if (y >= GAME_UI_SCREEN_HEIGHT - GAME_UI_SCREEN_RADIUS) {
        dy = y - (GAME_UI_SCREEN_HEIGHT - 1 - GAME_UI_SCREEN_RADIUS);
    } else {
        return 1;
    }
    return dx * dx + dy * dy <=
           GAME_UI_SCREEN_RADIUS * GAME_UI_SCREEN_RADIUS;
}

int main(void)
{
    // 180 px was too narrow for the second rendered line on the device.
    assert(GAME_UI_FOOTER_WIDTH >= 220);
    assert(GAME_UI_FOOTER_Y >= GAME_UI_PANEL_BOTTOM + 8);
    assert(GAME_UI_FOOTER_Y + GAME_UI_FOOTER_HEIGHT <= GAME_UI_SCREEN_HEIGHT);
    assert(point_is_inside_rounded_screen(GAME_UI_FOOTER_X,
                                          GAME_UI_FOOTER_Y));
    assert(point_is_inside_rounded_screen(
        GAME_UI_FOOTER_X + GAME_UI_FOOTER_WIDTH - 1,
        GAME_UI_FOOTER_Y));
    assert(point_is_inside_rounded_screen(
        GAME_UI_FOOTER_X,
        GAME_UI_FOOTER_Y + GAME_UI_FOOTER_HEIGHT - 1));
    assert(point_is_inside_rounded_screen(
        GAME_UI_FOOTER_X + GAME_UI_FOOTER_WIDTH - 1,
        GAME_UI_FOOTER_Y + GAME_UI_FOOTER_HEIGHT - 1));
    return 0;
}
