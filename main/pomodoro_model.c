#include "pomodoro_model.h"

#include <stddef.h>
#include <string.h>

static const uint8_t FOCUS_MINUTES[] = { 15, 25, 45 };
static const uint32_t CAT_THRESHOLDS[] = { 3, 8, 15, 25 };

static uint32_t seconds_until(uint64_t deadline_ms, uint64_t now_ms) {
    if (now_ms >= deadline_ms) return 0;
    return (uint32_t)((deadline_ms - now_ms + 999) / 1000);
}

static void update_running_remaining(pomodoro_model_t *model, uint64_t now_ms) {
    if (model->state == POMODORO_FOCUS_RUNNING) {
        model->remaining_sec = seconds_until(model->deadline_ms, now_ms);
    } else if (model->state == POMODORO_BREAK_RUNNING) {
        model->break_remaining_sec = seconds_until(model->deadline_ms, now_ms);
    }
}

void pomodoro_model_defaults(pomodoro_model_t *model) {
    if (!model) return;
    memset(model, 0, sizeof(*model));
    model->version = POMODORO_MODEL_VERSION;
    model->state = POMODORO_IDLE;
    model->selected_index = 1;
    model->remaining_sec = FOCUS_MINUTES[model->selected_index] * 60U;
}

void pomodoro_model_restore(pomodoro_model_t *model) {
    if (!model) return;

    if (model->version != POMODORO_MODEL_VERSION ||
        model->selected_index >= sizeof(FOCUS_MINUTES) ||
        model->state > POMODORO_BREAK_PAUSED) {
        pomodoro_model_defaults(model);
        return;
    }

    model->pomodoro_round %= 4;
    model->deadline_ms = 0;
    model->confirm_deadline_ms = 0;
    model->reward_deadline_ms = 0;

    if (model->reward_pending) {
        model->state = POMODORO_REWARD;
    } else if (model->state == POMODORO_FOCUS_RUNNING ||
               model->state == POMODORO_ABANDON_CONFIRM) {
        model->state = POMODORO_FOCUS_PAUSED;
    } else if (model->state == POMODORO_BREAK_RUNNING) {
        model->state = POMODORO_BREAK_PAUSED;
    } else if (model->state == POMODORO_REWARD) {
        model->state = POMODORO_BREAK_PROMPT;
    }

    if (model->remaining_sec == 0 &&
        (model->state == POMODORO_IDLE || model->state == POMODORO_FOCUS_PAUSED)) {
        model->remaining_sec = pomodoro_model_focus_min(model) * 60U;
    }
    if (model->pending_break_min != 5 && model->pending_break_min != 15) {
        model->pending_break_min = 5;
    }
}

uint8_t pomodoro_model_focus_min(const pomodoro_model_t *model) {
    if (!model || model->selected_index >= sizeof(FOCUS_MINUTES)) return 25;
    return FOCUS_MINUTES[model->selected_index];
}

bool pomodoro_model_select_duration(pomodoro_model_t *model, int direction) {
    if (!model || model->state != POMODORO_IDLE || direction == 0) return false;
    int index = (int)model->selected_index + (direction > 0 ? 1 : -1);
    if (index < 0) index = (int)sizeof(FOCUS_MINUTES) - 1;
    if (index >= (int)sizeof(FOCUS_MINUTES)) index = 0;
    model->selected_index = (uint8_t)index;
    model->remaining_sec = pomodoro_model_focus_min(model) * 60U;
    return true;
}

bool pomodoro_model_start_focus(pomodoro_model_t *model, uint64_t now_ms) {
    if (!model || model->state != POMODORO_IDLE) return false;
    model->remaining_sec = pomodoro_model_focus_min(model) * 60U;
    model->session_id++;
    model->deadline_ms = now_ms + model->remaining_sec * 1000ULL;
    model->state = POMODORO_FOCUS_RUNNING;
    return true;
}

bool pomodoro_model_pause(pomodoro_model_t *model, uint64_t now_ms) {
    if (!model) return false;
    update_running_remaining(model, now_ms);
    if (model->state == POMODORO_FOCUS_RUNNING) {
        model->state = POMODORO_FOCUS_PAUSED;
        model->deadline_ms = 0;
        return true;
    }
    if (model->state == POMODORO_BREAK_RUNNING) {
        model->state = POMODORO_BREAK_PAUSED;
        model->deadline_ms = 0;
        return true;
    }
    return false;
}

bool pomodoro_model_resume(pomodoro_model_t *model, uint64_t now_ms) {
    if (!model) return false;
    if (model->state == POMODORO_FOCUS_PAUSED && model->remaining_sec > 0) {
        model->state = POMODORO_FOCUS_RUNNING;
        model->deadline_ms = now_ms + model->remaining_sec * 1000ULL;
        return true;
    }
    if (model->state == POMODORO_BREAK_PAUSED && model->break_remaining_sec > 0) {
        model->state = POMODORO_BREAK_RUNNING;
        model->deadline_ms = now_ms + model->break_remaining_sec * 1000ULL;
        return true;
    }
    return false;
}

bool pomodoro_model_request_abandon(pomodoro_model_t *model, uint64_t now_ms) {
    if (!model || model->state != POMODORO_FOCUS_PAUSED) return false;
    model->state = POMODORO_ABANDON_CONFIRM;
    model->confirm_deadline_ms = now_ms + POMODORO_CONFIRM_MS;
    return true;
}

bool pomodoro_model_cancel_abandon(pomodoro_model_t *model) {
    if (!model || model->state != POMODORO_ABANDON_CONFIRM) return false;
    model->state = POMODORO_FOCUS_PAUSED;
    model->confirm_deadline_ms = 0;
    return true;
}

bool pomodoro_model_confirm_abandon(pomodoro_model_t *model) {
    if (!model || model->state != POMODORO_ABANDON_CONFIRM) return false;
    model->state = POMODORO_IDLE;
    model->remaining_sec = pomodoro_model_focus_min(model) * 60U;
    model->confirm_deadline_ms = 0;
    return true;
}

bool pomodoro_model_start_break(pomodoro_model_t *model, uint64_t now_ms) {
    if (!model || model->state != POMODORO_BREAK_PROMPT) return false;
    if (model->pending_break_min != 5 && model->pending_break_min != 15) {
        model->pending_break_min = 5;
    }
    model->break_remaining_sec = model->pending_break_min * 60U;
    model->deadline_ms = now_ms + model->break_remaining_sec * 1000ULL;
    model->state = POMODORO_BREAK_RUNNING;
    return true;
}

bool pomodoro_model_skip_break(pomodoro_model_t *model) {
    if (!model || model->state != POMODORO_BREAK_PROMPT) return false;
    model->state = POMODORO_IDLE;
    model->break_remaining_sec = 0;
    model->remaining_sec = pomodoro_model_focus_min(model) * 60U;
    return true;
}

pomodoro_event_t pomodoro_model_tick(pomodoro_model_t *model, uint64_t now_ms) {
    if (!model) return POMODORO_EVENT_NONE;

    update_running_remaining(model, now_ms);
    if (model->state == POMODORO_FOCUS_RUNNING && model->remaining_sec == 0) {
        model->completed_sessions++;
        model->completed_focus_min += pomodoro_model_focus_min(model);
        model->pending_break_min = (model->pomodoro_round == 3) ? 15 : 5;
        model->pomodoro_round = (model->pomodoro_round + 1) % 4;
        model->reward_pending = true;
        model->state = POMODORO_REWARD;
        model->deadline_ms = 0;
        model->reward_deadline_ms = now_ms + POMODORO_REWARD_MS;
        return POMODORO_EVENT_FOCUS_COMPLETE;
    }

    if (model->state == POMODORO_REWARD) {
        if (model->reward_deadline_ms == 0) {
            model->reward_deadline_ms = now_ms + POMODORO_REWARD_MS;
        } else if (now_ms >= model->reward_deadline_ms) {
            model->reward_pending = false;
            model->state = POMODORO_BREAK_PROMPT;
            model->reward_deadline_ms = 0;
            return POMODORO_EVENT_REWARD_FINISHED;
        }
    }

    if (model->state == POMODORO_BREAK_RUNNING && model->break_remaining_sec == 0) {
        model->state = POMODORO_IDLE;
        model->remaining_sec = pomodoro_model_focus_min(model) * 60U;
        model->deadline_ms = 0;
        return POMODORO_EVENT_BREAK_COMPLETE;
    }

    if (model->state == POMODORO_ABANDON_CONFIRM &&
        now_ms >= model->confirm_deadline_ms) {
        model->state = POMODORO_FOCUS_PAUSED;
        model->confirm_deadline_ms = 0;
        return POMODORO_EVENT_CONFIRM_TIMEOUT;
    }
    return POMODORO_EVENT_NONE;
}

uint8_t pomodoro_model_cat_stage(const pomodoro_model_t *model) {
    uint32_t completed = model ? model->completed_sessions : 0;
    for (uint8_t i = 0; i < sizeof(CAT_THRESHOLDS) / sizeof(CAT_THRESHOLDS[0]); i++) {
        if (completed < CAT_THRESHOLDS[i]) return i;
    }
    return 4;
}

void pomodoro_model_growth(const pomodoro_model_t *model,
                           uint32_t *current, uint32_t *target) {
    uint32_t completed = model ? model->completed_sessions : 0;
    uint8_t stage = pomodoro_model_cat_stage(model);
    uint32_t lower = stage == 0 ? 0 : CAT_THRESHOLDS[stage - 1];
    uint32_t upper = stage < 4 ? CAT_THRESHOLDS[stage] : lower + 5;
    if (stage == 4) lower += ((completed - lower) / 5) * 5;
    if (current) *current = completed - lower;
    if (target) *target = upper - (stage == 4 ? CAT_THRESHOLDS[3] : lower);
    if (stage == 4 && target) *target = 5;
}
