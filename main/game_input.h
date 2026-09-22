#pragma once

#include "game_model.h"

typedef enum {
    GAME_BUTTON_UP = 0,
    GAME_BUTTON_DOWN,
    GAME_BUTTON_OK,
} game_button_t;

typedef enum {
    GAME_BUTTON_EVENT_PRESS = 0,
    GAME_BUTTON_EVENT_CLICK,
    GAME_BUTTON_EVENT_DOUBLE,
    GAME_BUTTON_EVENT_LONG,
} game_button_event_t;

game_action_t game_input_map(game_button_t button, game_button_event_t event);
