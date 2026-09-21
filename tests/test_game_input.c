#include <assert.h>
#include <stdio.h>

#include "game_input.h"

int main(void)
{
    assert(game_input_map(GAME_BUTTON_UP, GAME_BUTTON_EVENT_CLICK) == GAME_ACTION_UP);
    assert(game_input_map(GAME_BUTTON_DOWN, GAME_BUTTON_EVENT_CLICK) == GAME_ACTION_DOWN);
    assert(game_input_map(GAME_BUTTON_OK, GAME_BUTTON_EVENT_CLICK) == GAME_ACTION_CONFIRM);
    assert(game_input_map(GAME_BUTTON_OK, GAME_BUTTON_EVENT_LONG) == GAME_ACTION_BACK);

    assert(game_input_map(GAME_BUTTON_UP, GAME_BUTTON_EVENT_LONG) == GAME_ACTION_NONE);
    assert(game_input_map(GAME_BUTTON_DOWN, GAME_BUTTON_EVENT_LONG) == GAME_ACTION_NONE);
    assert(game_input_map(GAME_BUTTON_OK, GAME_BUTTON_EVENT_PRESS) == GAME_ACTION_NONE);
    assert(game_input_map(GAME_BUTTON_OK, GAME_BUTTON_EVENT_DOUBLE) == GAME_ACTION_NONE);

    puts("game_input: all tests passed");
    return 0;
}
