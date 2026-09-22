#include "game_input.h"

game_action_t game_input_map(game_button_t button, game_button_event_t event)
{
    if (event == GAME_BUTTON_EVENT_LONG && button == GAME_BUTTON_OK) {
        return GAME_ACTION_BACK;
    }
    if (event != GAME_BUTTON_EVENT_CLICK) return GAME_ACTION_NONE;

    if (button == GAME_BUTTON_UP) return GAME_ACTION_UP;
    if (button == GAME_BUTTON_DOWN) return GAME_ACTION_DOWN;
    if (button == GAME_BUTTON_OK) return GAME_ACTION_CONFIRM;
    return GAME_ACTION_NONE;
}
