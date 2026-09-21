#include <assert.h>
#include <stdio.h>

#include "game_model.h"

static void action(game_model_t *model, game_action_t input)
{
    game_model_handle_action(model, input, 1000);
}

static void enter_code(game_model_t *model, const uint8_t *digits, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        for (uint8_t value = 0; value < digits[i]; ++value) action(model, GAME_ACTION_UP);
        action(model, GAME_ACTION_CONFIRM);
    }
}

static void start_game(game_model_t *model)
{
    game_model_init(model);
    assert(model->current_scene == GAME_SCENE_BOOT);
    game_model_boot_complete(model);
    assert(model->current_scene == GAME_SCENE_INTRO);
    action(model, GAME_ACTION_CONFIRM);
    assert(model->current_scene == GAME_SCENE_ROOM_1);
    assert(model->remaining_time == GAME_DURATION_SECONDS);
}

static void open_puzzle_1(game_model_t *model)
{
    for (int i = 0; i < 3; ++i) action(model, GAME_ACTION_DOWN);
    action(model, GAME_ACTION_CONFIRM);
    assert(model->current_scene == GAME_SCENE_PUZZLE_1);
    assert(model->code_length == 3);
}

static void solve_puzzle_1(game_model_t *model)
{
    const uint8_t answer[] = { 7, 1, 3 };
    open_puzzle_1(model);
    enter_code(model, answer, 3);
    assert(model->puzzle1_solved);
    assert(model->current_scene == GAME_SCENE_ROOM_2);
}

static void solve_puzzle_2(game_model_t *model)
{
    const uint8_t answer[] = { 1, 0, 0, 1 };
    action(model, GAME_ACTION_CONFIRM);
    assert(model->current_scene == GAME_SCENE_PUZZLE_2);
    enter_code(model, answer, 4);
    assert(model->puzzle2_solved);
    assert(model->current_scene == GAME_SCENE_FINAL_ROOM);
}

static void reach_final_choice(game_model_t *model)
{
    start_game(model);
    solve_puzzle_1(model);
    solve_puzzle_2(model);
    action(model, GAME_ACTION_DOWN);
    action(model, GAME_ACTION_DOWN);
    action(model, GAME_ACTION_CONFIRM);
    assert(model->current_view == GAME_VIEW_FINAL_PASSPORT);
    action(model, GAME_ACTION_CONFIRM);
    assert(model->current_scene == GAME_SCENE_FINAL_CHOICE);
}

static void test_navigation_and_back(void)
{
    static const game_view_t ok_back_views[] = {
        GAME_VIEW_CLOCK,
        GAME_VIEW_BOOK,
        GAME_VIEW_PHOTO,
        GAME_VIEW_ROOM_1_PASSPORT,
        GAME_VIEW_PASSPORT_LOG,
        GAME_VIEW_ROOM_2_PASSPORT,
        GAME_VIEW_FINAL_ROOM,
        GAME_VIEW_FINAL_DOOR,
    };
    game_model_t model;
    start_game(&model);

    for (size_t i = 0; i < sizeof(ok_back_views) / sizeof(ok_back_views[0]); ++i) {
        model.current_view = ok_back_views[i];
        assert(game_model_ok_returns_from_view(&model));
        action(&model, GAME_ACTION_CONFIRM);
        assert(model.current_view == GAME_VIEW_ROOT);
        assert(!model.system_menu_open);
    }

    model.current_view = GAME_VIEW_CLOCK;
    action(&model, GAME_ACTION_BACK);
    assert(model.current_view == GAME_VIEW_ROOT);
    assert(!model.system_menu_open);
    action(&model, GAME_ACTION_BACK);
    assert(model.system_menu_open);
    assert(model.system_menu_selection == GAME_SYSTEM_RESTART);
    action(&model, GAME_ACTION_BACK);
    assert(!model.system_menu_open);

    open_puzzle_1(&model);
    assert(!game_model_ok_returns_from_view(&model));
    action(&model, GAME_ACTION_BACK);
    assert(model.current_scene == GAME_SCENE_ROOM_1);

    model.current_scene = GAME_SCENE_FINAL_ROOM;
    model.current_view = GAME_VIEW_FINAL_PASSPORT;
    assert(!game_model_ok_returns_from_view(&model));
    action(&model, GAME_ACTION_CONFIRM);
    assert(model.current_scene == GAME_SCENE_FINAL_CHOICE);
    assert(model.current_view == GAME_VIEW_ROOT);
    assert(!game_model_ok_returns_from_view(NULL));
}

static void test_puzzle_feedback_and_progression(void)
{
    game_model_t model;
    const uint8_t wrong_1[] = { 8, 1, 7 };
    const uint8_t wrong_2[] = { 1, 7, 3 };
    const uint8_t wrong_3[] = { 0, 0, 0 };
    const uint8_t trap[] = { 4, 8, 2, 1 };

    start_game(&model);
    open_puzzle_1(&model);
    enter_code(&model, wrong_1, 3);
    assert(model.wrong_attempts == 1);
    assert(model.feedback == GAME_FEEDBACK_PUZZLE_1_WRONG);
    enter_code(&model, wrong_2, 3);
    assert(model.feedback == GAME_FEEDBACK_PUZZLE_1_DIRECT_JOIN);
    enter_code(&model, wrong_3, 3);
    assert(model.feedback == GAME_FEEDBACK_PUZZLE_1_LOOK_AGAIN);

    const uint8_t answer_1[] = { 7, 1, 3 };
    enter_code(&model, answer_1, 3);
    assert(model.current_scene == GAME_SCENE_ROOM_2);
    assert(model.wrong_attempts == 0);
    assert(model.feedback == GAME_FEEDBACK_CORRECT);

    action(&model, GAME_ACTION_DOWN);
    assert(model.feedback == GAME_FEEDBACK_NONE);
    action(&model, GAME_ACTION_UP);

    action(&model, GAME_ACTION_CONFIRM);
    enter_code(&model, trap, 4);
    assert(model.current_scene == GAME_SCENE_PUZZLE_2);
    assert(model.feedback == GAME_FEEDBACK_PASSPORT_TRICK);
    assert(model.ending == GAME_ENDING_NONE);

    const uint8_t answer_2[] = { 1, 0, 0, 1 };
    enter_code(&model, answer_2, 4);
    assert(model.current_scene == GAME_SCENE_FINAL_ROOM);
    assert(model.feedback == GAME_FEEDBACK_CORRECT);
    action(&model, GAME_ACTION_DOWN);
    assert(model.feedback == GAME_FEEDBACK_NONE);
}

static void test_system_menu_does_not_pause_time(void)
{
    game_model_t model;
    start_game(&model);
    action(&model, GAME_ACTION_BACK);
    assert(model.system_menu_open);

    assert(game_model_tick(&model, 601000) == GAME_TIMER_EVENT_FIVE_MINUTES);
    assert(model.remaining_time == 300);
    assert(model.system_menu_open);

    action(&model, GAME_ACTION_DOWN);
    assert(model.system_menu_selection == GAME_SYSTEM_CONTINUE);
    action(&model, GAME_ACTION_CONFIRM);
    assert(!model.system_menu_open);
    assert(model.current_scene == GAME_SCENE_ROOM_1);

    action(&model, GAME_ACTION_BACK);
    action(&model, GAME_ACTION_CONFIRM);
    assert(model.current_scene == GAME_SCENE_INTRO);
    assert(!model.timer.started);
}

static void test_language_settings_and_runtime_restart(void)
{
    game_model_t model;
    game_model_init(&model);
    game_model_boot_complete(&model);
    assert(model.language == GAME_LANGUAGE_ZH_CN);

    action(&model, GAME_ACTION_DOWN);
    assert(model.intro_selection == GAME_INTRO_SETTINGS);
    action(&model, GAME_ACTION_CONFIRM);
    assert(model.settings_menu_open);
    assert(!model.settings_return_to_system);
    assert(model.settings_selection == GAME_LANGUAGE_ZH_CN);

    action(&model, GAME_ACTION_DOWN);
    action(&model, GAME_ACTION_CONFIRM);
    assert(!model.settings_menu_open);
    assert(model.current_scene == GAME_SCENE_INTRO);
    assert(model.language == GAME_LANGUAGE_ENGLISH);

    action(&model, GAME_ACTION_UP);
    action(&model, GAME_ACTION_CONFIRM);
    assert(model.current_scene == GAME_SCENE_ROOM_1);
    action(&model, GAME_ACTION_BACK);
    action(&model, GAME_ACTION_DOWN);
    action(&model, GAME_ACTION_DOWN);
    assert(model.system_menu_selection == GAME_SYSTEM_SETTINGS);
    action(&model, GAME_ACTION_CONFIRM);
    assert(model.settings_menu_open);
    assert(model.settings_return_to_system);
    assert(model.settings_selection == GAME_LANGUAGE_ENGLISH);

    assert(game_model_tick(&model, 6000) == GAME_TIMER_EVENT_NONE);
    assert(model.remaining_time == GAME_DURATION_SECONDS - 5U);
    action(&model, GAME_ACTION_UP);
    action(&model, GAME_ACTION_CONFIRM);
    assert(model.language == GAME_LANGUAGE_ZH_CN);
    assert(!model.settings_menu_open);
    assert(model.system_menu_open);

    action(&model, GAME_ACTION_BACK);
    action(&model, GAME_ACTION_BACK);
    action(&model, GAME_ACTION_CONFIRM);
    assert(model.current_scene == GAME_SCENE_INTRO);
    assert(model.language == GAME_LANGUAGE_ZH_CN);
    assert(!model.timer.started);
}

static void test_hints_and_timeout(void)
{
    game_model_t model;
    start_game(&model);
    game_model_tick(&model, 301000);
    assert(model.remaining_time == 600 && model.hint_level == 1);
    game_model_tick(&model, 481000);
    assert(model.remaining_time == 420 && model.hint_level == 2);
    game_model_tick(&model, 661000);
    assert(model.remaining_time == 240 && model.hint_level == 3);

    action(&model, GAME_ACTION_BACK);
    assert(model.system_menu_open);
    uint8_t events = game_model_tick(&model, 901000);
    assert(events & GAME_TIMER_EVENT_TIMEOUT);
    assert(model.current_scene == GAME_SCENE_TIMEOUT);
    assert(!model.system_menu_open);
    action(&model, GAME_ACTION_CONFIRM);
    assert(model.current_scene == GAME_SCENE_INTRO);
}

static void test_all_endings(void)
{
    static const game_ending_t expected[] = {
        GAME_ENDING_GUIDED,
        GAME_ENDING_GAME_OVER,
        GAME_ENDING_ESCAPED,
        GAME_ENDING_TRUE_ESCAPE,
    };
    for (unsigned choice = 0; choice < 4; ++choice) {
        game_model_t model;
        reach_final_choice(&model);
        for (unsigned i = 0; i < choice; ++i) action(&model, GAME_ACTION_DOWN);
        action(&model, GAME_ACTION_CONFIRM);
        assert(model.current_scene == GAME_SCENE_ENDING);
        assert(model.ending == expected[choice]);
        assert(model.puzzle3_choice == (game_final_choice_t)(GAME_FINAL_CHOICE_A + choice));
        assert(model.remaining_time == GAME_DURATION_SECONDS);
        assert(game_model_tick(&model, 9999999) == GAME_TIMER_EVENT_NONE);
        assert(model.current_scene == GAME_SCENE_ENDING);
    }
}

int main(void)
{
    test_navigation_and_back();
    test_puzzle_feedback_and_progression();
    test_system_menu_does_not_pause_time();
    test_language_settings_and_runtime_restart();
    test_hints_and_timeout();
    test_all_endings();
    puts("game_model: all tests passed");
    return 0;
}
