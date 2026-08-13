#include "pomodoro_model.h"

#include <assert.h>
#include <stdio.h>

static void test_defaults_and_selection(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    assert(model.state == POMODORO_IDLE);
    assert(pomodoro_model_focus_min(&model) == 25);
    assert(model.remaining_sec == 25 * 60);
    assert(pomodoro_model_select_duration(&model, 1));
    assert(pomodoro_model_focus_min(&model) == 45);
    assert(pomodoro_model_select_duration(&model, 1));
    assert(pomodoro_model_focus_min(&model) == 15);
}

static void test_focus_pause_resume_and_completion(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    assert(pomodoro_model_start_focus(&model, 1000));
    assert(model.session_id == 1);
    assert(pomodoro_model_pause(&model, 11000));
    assert(model.remaining_sec == 1490);
    assert(pomodoro_model_resume(&model, 20000));
    assert(pomodoro_model_tick(&model, 1509999) == POMODORO_EVENT_NONE);
    assert(pomodoro_model_tick(&model, 1510000) == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(model.completed_sessions == 1);
    assert(model.completed_focus_min == 25);
    assert(model.state == POMODORO_REWARD);
    assert(pomodoro_model_tick(&model, 1513000) == POMODORO_EVENT_REWARD_FINISHED);
    assert(model.state == POMODORO_BREAK_PROMPT);
    assert(pomodoro_model_tick(&model, 1514000) == POMODORO_EVENT_NONE);
    assert(model.completed_sessions == 1);
}

static void test_abandon_timeout_and_confirm(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    pomodoro_model_start_focus(&model, 0);
    pomodoro_model_pause(&model, 1000);
    assert(pomodoro_model_request_abandon(&model, 2000));
    assert(pomodoro_model_tick(&model, 6999) == POMODORO_EVENT_NONE);
    assert(pomodoro_model_tick(&model, 7000) == POMODORO_EVENT_CONFIRM_TIMEOUT);
    assert(model.state == POMODORO_FOCUS_PAUSED);
    assert(pomodoro_model_request_abandon(&model, 8000));
    assert(pomodoro_model_confirm_abandon(&model));
    assert(model.state == POMODORO_IDLE);
}

static void complete_focus(pomodoro_model_t *model, uint64_t now_ms) {
    assert(pomodoro_model_start_focus(model, now_ms));
    uint64_t end_ms = now_ms + pomodoro_model_focus_min(model) * 60000ULL;
    assert(pomodoro_model_tick(model, end_ms) == POMODORO_EVENT_FOCUS_COMPLETE);
    assert(pomodoro_model_tick(model, end_ms + POMODORO_REWARD_MS) ==
           POMODORO_EVENT_REWARD_FINISHED);
}

static void test_break_cycle_and_growth(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    uint64_t now_ms = 0;
    for (int i = 0; i < 4; i++) {
        complete_focus(&model, now_ms);
        assert(model.pending_break_min == (i == 3 ? 15 : 5));
        assert(pomodoro_model_skip_break(&model));
        now_ms += 2000000;
    }
    assert(model.pomodoro_round == 0);
    assert(model.completed_sessions == 4);
    assert(pomodoro_model_cat_stage(&model) == 1);
    uint32_t current, target;
    pomodoro_model_growth(&model, &current, &target);
    assert(current == 1 && target == 5);

    static const uint32_t sessions[] = { 0, 2, 3, 7, 8, 14, 15, 24, 25, 30 };
    static const uint8_t stages[] =   { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4 };
    for (size_t i = 0; i < sizeof(sessions) / sizeof(sessions[0]); i++) {
        model.completed_sessions = sessions[i];
        assert(pomodoro_model_cat_stage(&model) == stages[i]);
    }
}

static void test_break_timer(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    model.state = POMODORO_BREAK_PROMPT;
    model.pending_break_min = 5;
    assert(pomodoro_model_start_break(&model, 1000));
    assert(pomodoro_model_tick(&model, 300999) == POMODORO_EVENT_NONE);
    assert(pomodoro_model_tick(&model, 301000) == POMODORO_EVENT_BREAK_COMPLETE);
    assert(model.state == POMODORO_IDLE);
}

static void test_restore_is_safe(void) {
    pomodoro_model_t model;
    pomodoro_model_defaults(&model);
    model.state = POMODORO_FOCUS_RUNNING;
    model.remaining_sec = 321;
    model.deadline_ms = 999999;
    pomodoro_model_restore(&model);
    assert(model.state == POMODORO_FOCUS_PAUSED);
    assert(model.remaining_sec == 321);
    assert(model.deadline_ms == 0);

    model.state = POMODORO_IDLE;
    model.reward_pending = true;
    pomodoro_model_restore(&model);
    assert(model.state == POMODORO_REWARD);
}

int main(void) {
    test_defaults_and_selection();
    test_focus_pause_resume_and_completion();
    test_abandon_timeout_and_confirm();
    test_break_cycle_and_growth();
    test_break_timer();
    test_restore_is_safe();
    puts("pomodoro_model: all tests passed");
    return 0;
}
