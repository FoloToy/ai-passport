#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "game_i18n.h"
#include "game_puzzles.h"
#include "game_timer.h"

#define GAME_MAX_CODE_DIGITS 4
#define GAME_COVER_DURATION_MS 10000ULL

typedef enum {
    GAME_SCENE_BOOT = 0,
    GAME_SCENE_COVER,
    GAME_SCENE_INTRO,
    GAME_SCENE_ROOM_1,
    GAME_SCENE_PUZZLE_1,
    GAME_SCENE_ROOM_2,
    GAME_SCENE_PUZZLE_2,
    GAME_SCENE_FINAL_ROOM,
    GAME_SCENE_FINAL_CHOICE,
    GAME_SCENE_ENDING,
    GAME_SCENE_TIMEOUT,
} game_scene_t;

typedef enum {
    GAME_VIEW_ROOT = 0,
    GAME_VIEW_CLOCK,
    GAME_VIEW_BOOK,
    GAME_VIEW_PHOTO,
    GAME_VIEW_ROOM_1_PASSPORT,
    GAME_VIEW_PASSPORT_LOG,
    GAME_VIEW_ROOM_2_PASSPORT,
    GAME_VIEW_FINAL_ROOM,
    GAME_VIEW_FINAL_DOOR,
    GAME_VIEW_FINAL_PASSPORT,
} game_view_t;

typedef enum {
    GAME_ACTION_NONE = 0,
    GAME_ACTION_UP,
    GAME_ACTION_DOWN,
    GAME_ACTION_CONFIRM,
    GAME_ACTION_BACK,
} game_action_t;

typedef enum {
    GAME_FEEDBACK_NONE = 0,
    GAME_FEEDBACK_PUZZLE_1_WRONG,
    GAME_FEEDBACK_PUZZLE_1_DIRECT_JOIN,
    GAME_FEEDBACK_PUZZLE_1_LOOK_AGAIN,
    GAME_FEEDBACK_PUZZLE_2_WRONG,
    GAME_FEEDBACK_PASSPORT_TRICK,
    GAME_FEEDBACK_CORRECT,
} game_feedback_t;

typedef enum {
    GAME_SYSTEM_RESTART = 0,
    GAME_SYSTEM_CONTINUE,
    GAME_SYSTEM_SETTINGS,
    GAME_SYSTEM_ITEM_COUNT,
} game_system_item_t;

typedef enum {
    GAME_INTRO_START = 0,
    GAME_INTRO_SETTINGS,
    GAME_INTRO_ITEM_COUNT,
} game_intro_item_t;

typedef struct {
    game_scene_t current_scene;
    game_view_t current_view;
    game_ending_t ending;
    game_final_choice_t puzzle3_choice;
    game_feedback_t feedback;
    game_language_t language;
    game_timer_t timer;
    uint32_t remaining_time;
    uint64_t cover_started_ms;
    uint8_t hint_level;
    uint8_t wrong_attempts;
    uint8_t intro_selection;
    uint8_t room_selection;
    uint8_t final_choice_selection;
    uint8_t system_menu_selection;
    uint8_t settings_selection;
    uint8_t code_digits[GAME_MAX_CODE_DIGITS];
    uint8_t code_length;
    uint8_t code_cursor;
    uint8_t last_timer_events;
    bool puzzle1_solved;
    bool puzzle2_solved;
    bool system_menu_open;
    bool settings_menu_open;
    bool settings_return_to_system;
} game_model_t;

void game_model_init(game_model_t *model);
void game_model_boot_complete(game_model_t *model, uint64_t now_ms);
bool game_model_ok_returns_from_view(const game_model_t *model);
void game_model_handle_action(game_model_t *model, game_action_t action, uint64_t now_ms);
uint8_t game_model_tick(game_model_t *model, uint64_t now_ms);
