#pragma once

#include <stdbool.h>
#include <stdint.h>

#define POMODORO_MODEL_VERSION 1
#define POMODORO_REWARD_MS 3000
#define POMODORO_CONFIRM_MS 5000

typedef enum {
    POMODORO_IDLE = 0,
    POMODORO_FOCUS_RUNNING,
    POMODORO_FOCUS_PAUSED,
    POMODORO_ABANDON_CONFIRM,
    POMODORO_REWARD,
    POMODORO_BREAK_PROMPT,
    POMODORO_BREAK_RUNNING,
    POMODORO_BREAK_PAUSED,
} pomodoro_state_t;

typedef enum {
    POMODORO_EVENT_NONE = 0,
    POMODORO_EVENT_FOCUS_COMPLETE,
    POMODORO_EVENT_REWARD_FINISHED,
    POMODORO_EVENT_BREAK_COMPLETE,
    POMODORO_EVENT_CONFIRM_TIMEOUT,
} pomodoro_event_t;

typedef struct {
    uint16_t version;
    pomodoro_state_t state;
    uint8_t selected_index;
    uint32_t remaining_sec;
    uint32_t session_id;
    bool reward_pending;
    uint32_t completed_sessions;
    uint32_t completed_focus_min;
    uint8_t pomodoro_round;
    uint8_t pending_break_min;
    uint32_t break_remaining_sec;
    bool muted;

    /* 下面是运行时字段，不写入 NVS。 */
    uint64_t deadline_ms;
    uint64_t confirm_deadline_ms;
    uint64_t reward_deadline_ms;
} pomodoro_model_t;

void pomodoro_model_defaults(pomodoro_model_t *model);
void pomodoro_model_restore(pomodoro_model_t *model);

uint8_t pomodoro_model_focus_min(const pomodoro_model_t *model);
bool pomodoro_model_select_duration(pomodoro_model_t *model, int direction);

bool pomodoro_model_start_focus(pomodoro_model_t *model, uint64_t now_ms);
bool pomodoro_model_pause(pomodoro_model_t *model, uint64_t now_ms);
bool pomodoro_model_resume(pomodoro_model_t *model, uint64_t now_ms);
bool pomodoro_model_request_abandon(pomodoro_model_t *model, uint64_t now_ms);
bool pomodoro_model_cancel_abandon(pomodoro_model_t *model);
bool pomodoro_model_confirm_abandon(pomodoro_model_t *model);

bool pomodoro_model_start_break(pomodoro_model_t *model, uint64_t now_ms);
bool pomodoro_model_skip_break(pomodoro_model_t *model);

pomodoro_event_t pomodoro_model_tick(pomodoro_model_t *model, uint64_t now_ms);

uint8_t pomodoro_model_cat_stage(const pomodoro_model_t *model);
void pomodoro_model_growth(const pomodoro_model_t *model,
                           uint32_t *current, uint32_t *target);
