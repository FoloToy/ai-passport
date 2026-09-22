#include "game_model.h"

#include <string.h>

#define ROOM_1_ITEM_COUNT 5U
#define ROOM_2_ITEM_COUNT 3U
#define FINAL_ROOM_ITEM_COUNT 3U
#define FINAL_CHOICE_COUNT 4U

static uint8_t wrap_selection(uint8_t current, unsigned count, int direction)
{
    if (count == 0) return 0;
    if (direction < 0) return (uint8_t)((current + count - 1U) % count);
    return (uint8_t)((current + 1U) % count);
}

static void clear_code(game_model_t *model, uint8_t length)
{
    memset(model->code_digits, 0, sizeof(model->code_digits));
    model->code_length = length;
    model->code_cursor = 0;
}

static void reset_to_intro(game_model_t *model)
{
    game_language_t language = model->language;
    game_model_init(model);
    model->language = language;
    model->current_scene = GAME_SCENE_INTRO;
}

void game_model_init(game_model_t *model)
{
    if (!model) return;
    memset(model, 0, sizeof(*model));
    model->current_scene = GAME_SCENE_BOOT;
    model->current_view = GAME_VIEW_ROOT;
    model->puzzle3_choice = GAME_FINAL_CHOICE_NONE;
    model->ending = GAME_ENDING_NONE;
    model->language = GAME_LANGUAGE_ZH_CN;
    model->remaining_time = GAME_DURATION_SECONDS;
    game_timer_init(&model->timer, GAME_DURATION_SECONDS);
}

void game_model_boot_complete(game_model_t *model, uint64_t now_ms)
{
    if (model && model->current_scene == GAME_SCENE_BOOT) {
        model->current_scene = GAME_SCENE_COVER;
        model->cover_started_ms = now_ms;
    }
}

static void start_game(game_model_t *model, uint64_t now_ms)
{
    model->current_scene = GAME_SCENE_ROOM_1;
    model->current_view = GAME_VIEW_ROOT;
    model->room_selection = 0;
    model->remaining_time = GAME_DURATION_SECONDS;
    game_timer_start(&model->timer, now_ms);
}

static void open_system_menu(game_model_t *model)
{
    model->system_menu_open = true;
    model->system_menu_selection = GAME_SYSTEM_RESTART;
}

static void open_settings(game_model_t *model, bool return_to_system)
{
    model->settings_menu_open = true;
    model->settings_return_to_system = return_to_system;
    model->settings_selection = (uint8_t)model->language;
    model->system_menu_open = false;
}

static void close_settings(game_model_t *model)
{
    model->settings_menu_open = false;
    if (model->settings_return_to_system) model->system_menu_open = true;
    model->settings_return_to_system = false;
}

static void handle_settings_menu(game_model_t *model, game_action_t action)
{
    if (action == GAME_ACTION_BACK) {
        close_settings(model);
    } else if (action == GAME_ACTION_UP) {
        model->settings_selection = wrap_selection(
            model->settings_selection, GAME_LANGUAGE_COUNT, -1);
    } else if (action == GAME_ACTION_DOWN) {
        model->settings_selection = wrap_selection(
            model->settings_selection, GAME_LANGUAGE_COUNT, 1);
    } else if (action == GAME_ACTION_CONFIRM) {
        model->language = (game_language_t)model->settings_selection;
        close_settings(model);
    }
}

static void handle_system_menu(game_model_t *model, game_action_t action)
{
    if (action == GAME_ACTION_BACK) {
        model->system_menu_open = false;
    } else if (action == GAME_ACTION_UP) {
        model->system_menu_selection = wrap_selection(
            model->system_menu_selection, GAME_SYSTEM_ITEM_COUNT, -1);
    } else if (action == GAME_ACTION_DOWN) {
        model->system_menu_selection = wrap_selection(
            model->system_menu_selection, GAME_SYSTEM_ITEM_COUNT, 1);
    } else if (action == GAME_ACTION_CONFIRM) {
        if (model->system_menu_selection == GAME_SYSTEM_RESTART) {
            reset_to_intro(model);
        } else if (model->system_menu_selection == GAME_SYSTEM_CONTINUE) {
            model->system_menu_open = false;
        } else {
            open_settings(model, true);
        }
    }
}

static void handle_intro(game_model_t *model, game_action_t action, uint64_t now_ms)
{
    if (action == GAME_ACTION_UP) {
        model->intro_selection = wrap_selection(
            model->intro_selection, GAME_INTRO_ITEM_COUNT, -1);
    } else if (action == GAME_ACTION_DOWN) {
        model->intro_selection = wrap_selection(
            model->intro_selection, GAME_INTRO_ITEM_COUNT, 1);
    } else if (action == GAME_ACTION_CONFIRM) {
        if (model->intro_selection == GAME_INTRO_START) start_game(model, now_ms);
        else open_settings(model, false);
    }
}

static void handle_back(game_model_t *model)
{
    if (model->current_scene == GAME_SCENE_PUZZLE_1) {
        model->current_scene = GAME_SCENE_ROOM_1;
        model->current_view = GAME_VIEW_ROOT;
        return;
    }
    if (model->current_scene == GAME_SCENE_PUZZLE_2) {
        model->current_scene = GAME_SCENE_ROOM_2;
        model->current_view = GAME_VIEW_ROOT;
        return;
    }
    if (model->current_scene == GAME_SCENE_FINAL_CHOICE) {
        model->current_scene = GAME_SCENE_FINAL_ROOM;
        model->current_view = GAME_VIEW_ROOT;
        return;
    }
    if (model->current_view != GAME_VIEW_ROOT) {
        model->current_view = GAME_VIEW_ROOT;
        return;
    }
    if (model->current_scene >= GAME_SCENE_ROOM_1) open_system_menu(model);
}

bool game_model_ok_returns_from_view(const game_model_t *model)
{
    return model &&
           model->current_view != GAME_VIEW_ROOT &&
           model->current_view != GAME_VIEW_FINAL_PASSPORT;
}

static void open_room_1_selection(game_model_t *model)
{
    static const game_view_t views[] = {
        GAME_VIEW_CLOCK,
        GAME_VIEW_BOOK,
        GAME_VIEW_PHOTO,
        GAME_VIEW_ROOT,
        GAME_VIEW_ROOM_1_PASSPORT,
    };
    if (model->room_selection == 3) {
        model->current_scene = GAME_SCENE_PUZZLE_1;
        model->feedback = GAME_FEEDBACK_NONE;
        clear_code(model, 3);
    } else {
        model->current_view = views[model->room_selection];
    }
}

static void open_room_2_selection(game_model_t *model)
{
    if (model->room_selection == 0) {
        model->current_scene = GAME_SCENE_PUZZLE_2;
        model->feedback = GAME_FEEDBACK_NONE;
        clear_code(model, 4);
    } else if (model->room_selection == 1) {
        model->current_view = GAME_VIEW_PASSPORT_LOG;
    } else {
        model->current_view = GAME_VIEW_ROOM_2_PASSPORT;
    }
}

static void open_final_room_selection(game_model_t *model)
{
    static const game_view_t views[] = {
        GAME_VIEW_FINAL_ROOM,
        GAME_VIEW_FINAL_DOOR,
        GAME_VIEW_FINAL_PASSPORT,
    };
    model->current_view = views[model->room_selection];
}

static void submit_code(game_model_t *model)
{
    if (model->current_scene == GAME_SCENE_PUZZLE_1) {
        if (game_puzzle1_check(model->code_digits, model->code_length)) {
            model->puzzle1_solved = true;
            model->feedback = GAME_FEEDBACK_CORRECT;
            model->wrong_attempts = 0;
            clear_code(model, 0);
            model->current_scene = GAME_SCENE_ROOM_2;
            model->current_view = GAME_VIEW_ROOT;
            model->room_selection = 0;
            return;
        }
        if (model->wrong_attempts < UINT8_MAX) model->wrong_attempts++;
        if (model->wrong_attempts == 1) {
            model->feedback = GAME_FEEDBACK_PUZZLE_1_WRONG;
        } else if (model->wrong_attempts == 2) {
            model->feedback = GAME_FEEDBACK_PUZZLE_1_DIRECT_JOIN;
        } else {
            model->feedback = GAME_FEEDBACK_PUZZLE_1_LOOK_AGAIN;
        }
        clear_code(model, 3);
        return;
    }

    if (game_puzzle2_check(model->code_digits, model->code_length)) {
        model->puzzle2_solved = true;
        model->feedback = GAME_FEEDBACK_CORRECT;
        model->wrong_attempts = 0;
        clear_code(model, 0);
        model->current_scene = GAME_SCENE_FINAL_ROOM;
        model->current_view = GAME_VIEW_ROOT;
        model->room_selection = 0;
        return;
    }
    if (model->wrong_attempts < UINT8_MAX) model->wrong_attempts++;
    static const uint8_t trap_code[] = { 4, 8, 2, 1 };
    model->feedback = memcmp(model->code_digits, trap_code, sizeof(trap_code)) == 0
                    ? GAME_FEEDBACK_PASSPORT_TRICK
                    : GAME_FEEDBACK_PUZZLE_2_WRONG;
    clear_code(model, 4);
}

static void handle_code_action(game_model_t *model, game_action_t action)
{
    uint8_t *digit = &model->code_digits[model->code_cursor];
    if (action == GAME_ACTION_UP) {
        *digit = (uint8_t)((*digit + 1U) % 10U);
    } else if (action == GAME_ACTION_DOWN) {
        *digit = (uint8_t)((*digit + 9U) % 10U);
    } else if (action == GAME_ACTION_CONFIRM) {
        if (model->code_cursor + 1U < model->code_length) {
            model->code_cursor++;
        } else {
            submit_code(model);
        }
    }
}

static void handle_final_choice(game_model_t *model, game_action_t action, uint64_t now_ms)
{
    if (action == GAME_ACTION_UP) {
        model->final_choice_selection = wrap_selection(
            model->final_choice_selection, FINAL_CHOICE_COUNT, -1);
    } else if (action == GAME_ACTION_DOWN) {
        model->final_choice_selection = wrap_selection(
            model->final_choice_selection, FINAL_CHOICE_COUNT, 1);
    } else if (action == GAME_ACTION_CONFIRM) {
        model->puzzle3_choice = (game_final_choice_t)(
            GAME_FINAL_CHOICE_A + model->final_choice_selection);
        model->ending = game_ending_for_choice(model->puzzle3_choice);
        model->remaining_time = game_timer_remaining(&model->timer, now_ms);
        model->timer.duration_seconds = model->remaining_time;
        model->timer.started = false;
        model->current_scene = GAME_SCENE_ENDING;
        model->current_view = GAME_VIEW_ROOT;
    }
}

void game_model_handle_action(game_model_t *model, game_action_t action, uint64_t now_ms)
{
    if (!model || action == GAME_ACTION_NONE) return;
    if (model->feedback == GAME_FEEDBACK_CORRECT) {
        model->feedback = GAME_FEEDBACK_NONE;
    }
    if (model->settings_menu_open) {
        handle_settings_menu(model, action);
        return;
    }
    if (model->system_menu_open) {
        handle_system_menu(model, action);
        return;
    }
    if (model->current_scene == GAME_SCENE_COVER) {
        if (action == GAME_ACTION_CONFIRM) model->current_scene = GAME_SCENE_INTRO;
        return;
    }
    if (action == GAME_ACTION_BACK) {
        handle_back(model);
        return;
    }
    if (model->current_scene == GAME_SCENE_INTRO) {
        handle_intro(model, action, now_ms);
        return;
    }
    if (model->current_scene == GAME_SCENE_TIMEOUT) {
        if (action == GAME_ACTION_CONFIRM) reset_to_intro(model);
        return;
    }
    if (model->current_scene == GAME_SCENE_PUZZLE_1 ||
        model->current_scene == GAME_SCENE_PUZZLE_2) {
        handle_code_action(model, action);
        return;
    }
    if (model->current_scene == GAME_SCENE_FINAL_CHOICE) {
        handle_final_choice(model, action, now_ms);
        return;
    }
    if (model->current_view != GAME_VIEW_ROOT) {
        if (model->current_view == GAME_VIEW_FINAL_PASSPORT &&
            action == GAME_ACTION_CONFIRM) {
            model->current_scene = GAME_SCENE_FINAL_CHOICE;
            model->current_view = GAME_VIEW_ROOT;
            model->final_choice_selection = 0;
        } else if (action == GAME_ACTION_CONFIRM &&
                   game_model_ok_returns_from_view(model)) {
            handle_back(model);
        }
        return;
    }

    unsigned count = 0;
    if (model->current_scene == GAME_SCENE_ROOM_1) count = ROOM_1_ITEM_COUNT;
    if (model->current_scene == GAME_SCENE_ROOM_2) count = ROOM_2_ITEM_COUNT;
    if (model->current_scene == GAME_SCENE_FINAL_ROOM) count = FINAL_ROOM_ITEM_COUNT;
    if (count == 0) return;

    if (action == GAME_ACTION_UP) {
        model->room_selection = wrap_selection(model->room_selection, count, -1);
    } else if (action == GAME_ACTION_DOWN) {
        model->room_selection = wrap_selection(model->room_selection, count, 1);
    } else if (action == GAME_ACTION_CONFIRM) {
        if (model->current_scene == GAME_SCENE_ROOM_1) open_room_1_selection(model);
        else if (model->current_scene == GAME_SCENE_ROOM_2) open_room_2_selection(model);
        else open_final_room_selection(model);
    }
}

uint8_t game_model_tick(game_model_t *model, uint64_t now_ms)
{
    if (!model) return GAME_TIMER_EVENT_NONE;
    if (model->current_scene == GAME_SCENE_COVER) {
        if (now_ms - model->cover_started_ms >= GAME_COVER_DURATION_MS) {
            model->current_scene = GAME_SCENE_INTRO;
        }
        return GAME_TIMER_EVENT_NONE;
    }
    if (!model->timer.started) return GAME_TIMER_EVENT_NONE;
    uint8_t events = game_timer_poll(&model->timer, now_ms);
    model->remaining_time = game_timer_remaining(&model->timer, now_ms);
    model->last_timer_events = events;

    if (!model->puzzle1_solved) {
        uint8_t timed_hint = 0;
        if (model->remaining_time <= 10U * 60U) timed_hint = 1;
        if (model->remaining_time <= 7U * 60U) timed_hint = 2;
        if (model->remaining_time <= 4U * 60U) timed_hint = 3;
        if (timed_hint > model->hint_level) model->hint_level = timed_hint;
    }
    if (events & GAME_TIMER_EVENT_TIMEOUT) {
        model->current_scene = GAME_SCENE_TIMEOUT;
        model->current_view = GAME_VIEW_ROOT;
        model->system_menu_open = false;
        model->settings_menu_open = false;
        model->settings_return_to_system = false;
    }
    return events;
}
