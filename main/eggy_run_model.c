// eggy_run_model.c — pure Eggy Run logic.
#include "eggy_run_model.h"

// eggy_y tracks the BOTTOM edge; at rest the bottom sits on the ground.
#define REST_Y (EGGY_GROUND_Y)

static int32_t clamp_speed(int32_t score) {
    int32_t s = EGGY_SPEED_BASE + (score / 100) * EGGY_SPEED_PER_SCORE;
    if (s > EGGY_SPEED_MAX) {
        s = EGGY_SPEED_MAX;
    }
    return s;
}

static void spawn_obstacle(eggy_run_model_t *m) {
    int slot = -1;
    for (int i = 0; i < EGGY_MAX_OBSTACLES; i++) {
        if (!m->obstacles[i].active) {
            slot = i;
            break;
        }
    }
    if (slot < 0) {
        return;
    }
    eggy_obstacle_kind_t kind = (eggy_rng_range(&m->rng, 0, 2) == 0)
                                ? EGGY_OBSTACLE_SPIKE
                                : EGGY_OBSTACLE_BAR;
    m->obstacles[slot].kind = kind;
    m->obstacles[slot].x = EGGY_RUN_W + 4;
    m->obstacles[slot].active = true;
    m->obstacles[slot].passed = false;
}

void eggy_run_reset(eggy_run_model_t *m) {
    m->state = EGGY_RUN_READY;
    m->eggy_y = REST_Y;
    m->eggy_vy = 0;
    m->duck_ms = 0;
    m->score = 0;
    m->speed_pxps = EGGY_SPEED_BASE;
    m->spawn_timer_ms = 600;  // first obstacle a little after start
    m->elapsed_ms = 0;
    for (int i = 0; i < EGGY_MAX_OBSTACLES; i++) {
        m->obstacles[i].active = false;
    }
}

static bool overlap(int ax, int aw, int ay, int ah,
                    int bx, int bw, int by, int bh) {
    return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
}

static bool collides(const eggy_run_model_t *m) {
    int h = eggy_run_eggy_h(m);
    int top = (int)m->eggy_y - h;   // eggy box: [top, top+h] = [bottom-h, bottom]
    for (int i = 0; i < EGGY_MAX_OBSTACLES; i++) {
        if (!m->obstacles[i].active) {
            continue;
        }
        int ox = m->obstacles[i].x;
        int oy, oh;
        if (m->obstacles[i].kind == EGGY_OBSTACLE_SPIKE) {
            oy = EGGY_GROUND_Y - EGGY_SPIKE_H;
            oh = EGGY_SPIKE_H;
        } else {
            oy = EGGY_BAR_TOP;
            oh = EGGY_BAR_H;
        }
        if (overlap(EGGY_X, EGGY_W, top, h, ox, EGGY_OBSTACLE_W, oy, oh)) {
            return true;
        }
    }
    return false;
}

eggy_run_event_t eggy_run_step(eggy_run_model_t *m, uint32_t dt_ms,
                               eggy_run_input_t input) {
    eggy_run_event_t event = EGGY_RUN_EVENT_NONE;

    // Inputs are processed even in READY/GAME_OVER so the controller can react.
    if (input == EGGY_RUN_INPUT_START && m->state == EGGY_RUN_READY) {
        m->state = EGGY_RUN_PLAYING;
    }
    if (input == EGGY_RUN_INPUT_RESTART && m->state == EGGY_RUN_GAME_OVER) {
        eggy_run_reset(m);
        m->state = EGGY_RUN_PLAYING;
        return EGGY_RUN_EVENT_NONE;
    }
    if (input == EGGY_RUN_INPUT_TOGGLE_PAUSE) {
        if (m->state == EGGY_RUN_PLAYING) {
            m->state = EGGY_RUN_PAUSED;
            return EGGY_RUN_EVENT_NONE;
        }
        if (m->state == EGGY_RUN_PAUSED) {
            m->state = EGGY_RUN_PLAYING;
            return EGGY_RUN_EVENT_NONE;
        }
    }
    if (m->state != EGGY_RUN_PLAYING) {
        return EGGY_RUN_EVENT_NONE;
    }

    // Duck timer counts down in real time.
    if (m->duck_ms > 0) {
        if (dt_ms >= (uint32_t)m->duck_ms) {
            m->duck_ms = 0;
        } else {
            m->duck_ms -= (int16_t)dt_ms;
        }
    }
    if (input == EGGY_RUN_INPUT_DUCK && m->eggy_y >= REST_Y) {
        m->duck_ms = EGGY_DUCK_MS;
        event = EGGY_RUN_EVENT_DUCKED;
    }

    // Jump only when grounded (resting on the ground, not airborne).
    if (input == EGGY_RUN_INPUT_JUMP && m->eggy_y >= REST_Y) {
        m->eggy_vy = EGGY_JUMP_VY;
        event = EGGY_RUN_EVENT_JUMPED;
    }

    // Integrate vertical motion. vy += g*dt; y += vy*dt (dt in ms → /1000).
    m->eggy_vy += (int32_t)((EGGY_GRAVITY * (int32_t)dt_ms) / 1000);
    m->eggy_y += (int16_t)((m->eggy_vy * (int32_t)dt_ms) / 1000);
    if (m->eggy_y >= REST_Y) {
        m->eggy_y = REST_Y;
        m->eggy_vy = 0;
    }
    // Can't duck mid-air; if airborne, cancel duck window.
    if (m->eggy_y < REST_Y && m->duck_ms > 0) {
        m->duck_ms = 0;
    }

    // Scroll obstacles.
    int32_t move = (int32_t)((m->speed_pxps * (int32_t)dt_ms) / 1000);
    for (int i = 0; i < EGGY_MAX_OBSTACLES; i++) {
        if (!m->obstacles[i].active) {
            continue;
        }
        m->obstacles[i].x -= (int16_t)move;
        if (m->obstacles[i].x + EGGY_OBSTACLE_W < 0) {
            m->obstacles[i].active = false;
        } else if (!m->obstacles[i].passed &&
                   m->obstacles[i].x + EGGY_OBSTACLE_W < EGGY_X) {
            m->obstacles[i].passed = true;
            // passed-event kept implicit; score rewards it below.
        }
    }

    // Spawn pacing.
    if (m->spawn_timer_ms <= dt_ms) {
        spawn_obstacle(m);
        m->spawn_timer_ms = eggy_rng_range(&m->rng,
                                           EGGY_SPAWN_MIN_MS,
                                           EGGY_SPAWN_MAX_MS + 1);
    } else {
        m->spawn_timer_ms -= dt_ms;
    }

    // Score: ~half a point per px scrolled (view divides by 10 for display).
    m->score += (move + 1) / 2;

    // Difficulty ramps with score.
    m->speed_pxps = (uint32_t)clamp_speed(m->score);

    m->elapsed_ms += dt_ms;

    // Collision ends the run.
    if (collides(m)) {
        m->state = EGGY_RUN_GAME_OVER;
        return EGGY_RUN_EVENT_CRASHED;
    }
    return event;
}
