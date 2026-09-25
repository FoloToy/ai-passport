// tests/test_eggy_run_model.c — pure Eggy Run logic host test.
#include <assert.h>
#include <stdint.h>

#include "eggy_run_model.h"

static void start_run(eggy_run_model_t *m) {
    eggy_rng_seed(&m->rng, 42);
    eggy_run_reset(m);
    eggy_run_step(m, 16, EGGY_RUN_INPUT_START);  // READY -> PLAYING
}

static void step_n(eggy_run_model_t *m, int n, uint32_t dt,
                   eggy_run_input_t in) {
    for (int i = 0; i < n; i++) {
        eggy_run_step(m, dt, in);
    }
}

static void clear_obstacles(eggy_run_model_t *m) {
    for (int i = 0; i < EGGY_MAX_OBSTACLES; i++) {
        m->obstacles[i].active = false;
    }
}

int main(void) {
    eggy_run_model_t m;

    // Reset leaves eggy grounded at rest, READY (eggy_y is the bottom edge).
    eggy_rng_seed(&m.rng, 1);
    eggy_run_reset(&m);
    assert(m.state == EGGY_RUN_READY);
    assert(m.eggy_y == EGGY_GROUND_Y);

    // Jump only fires when grounded.
    start_run(&m);
    int16_t y0 = m.eggy_y;
    eggy_run_step(&m, 16, EGGY_RUN_INPUT_JUMP);
    assert(m.eggy_y < y0);            // rose
    assert(m.eggy_vy < 0);            // moving up
    int16_t vy_airborne = m.eggy_vy;
    // A second jump while airborne must not re-arm vy.
    eggy_run_step(&m, 16, EGGY_RUN_INPUT_JUMP);
    assert(m.eggy_vy != EGGY_JUMP_VY);
    assert(m.eggy_vy == vy_airborne + (int16_t)(EGGY_GRAVITY * 16 / 1000));

    // Eggy returns to the ground under gravity.
    step_n(&m, 80, 16, EGGY_RUN_INPUT_NONE);
    assert(m.eggy_y == EGGY_GROUND_Y);
    assert(m.eggy_vy == 0);

    // Grounded spike crash.
    start_run(&m);
    clear_obstacles(&m);
    m.obstacles[0].kind = EGGY_OBSTACLE_SPIKE;
    m.obstacles[0].x = EGGY_X;
    m.obstacles[0].active = true;
    m.obstacles[0].passed = false;
    eggy_run_event_t ev = eggy_run_step(&m, 16, EGGY_RUN_INPUT_NONE);
    assert(ev == EGGY_RUN_EVENT_CRASHED);
    assert(m.state == EGGY_RUN_GAME_OVER);

    // A high enough eggy clears a spike directly below (vertical clearance).
    start_run(&m);
    clear_obstacles(&m);
    // Jump and rise past the point where the eggy's bottom is above the spike.
    eggy_run_step(&m, 16, EGGY_RUN_INPUT_JUMP);
    step_n(&m, 10, 16, EGGY_RUN_INPUT_NONE);  // keep rising, no obstacles
    assert(m.eggy_y < EGGY_GROUND_Y - EGGY_SPIKE_H);  // bottom above spike top
    assert(m.state == EGGY_RUN_PLAYING);
    // Place a spike directly under the airborne eggy; it must not crash.
    m.obstacles[0].kind = EGGY_OBSTACLE_SPIKE;
    m.obstacles[0].x = EGGY_X;
    m.obstacles[0].active = true;
    m.obstacles[0].passed = false;
    ev = eggy_run_step(&m, 16, EGGY_RUN_INPUT_NONE);
    assert(ev != EGGY_RUN_EVENT_CRASHED);
    assert(m.state == EGGY_RUN_PLAYING);

    // Overhead bar: standing crashes, ducking clears.
    start_run(&m);
    clear_obstacles(&m);
    m.obstacles[0].kind = EGGY_OBSTACLE_BAR;
    m.obstacles[0].x = EGGY_X;
    m.obstacles[0].active = true;
    m.obstacles[0].passed = false;
    eggy_run_step(&m, 16, EGGY_RUN_INPUT_NONE);
    assert(m.state == EGGY_RUN_GAME_OVER);

    start_run(&m);
    clear_obstacles(&m);
    m.obstacles[0].kind = EGGY_OBSTACLE_BAR;
    m.obstacles[0].x = EGGY_X;
    m.obstacles[0].active = true;
    m.obstacles[0].passed = false;
    eggy_run_step(&m, 16, EGGY_RUN_INPUT_DUCK);
    assert(m.state == EGGY_RUN_PLAYING);
    assert(m.duck_ms > 0);

    // Score and difficulty increase over time.
    start_run(&m);
    clear_obstacles(&m);
    int32_t s0 = m.score;
    step_n(&m, 220, 16, EGGY_RUN_INPUT_NONE);
    assert(m.score > s0);
    assert(m.speed_pxps > EGGY_SPEED_BASE);

    // Pause toggles and freezes the score.
    start_run(&m);
    eggy_run_step(&m, 16, EGGY_RUN_INPUT_TOGGLE_PAUSE);
    assert(m.state == EGGY_RUN_PAUSED);
    int32_t paused_score = m.score;
    step_n(&m, 20, 16, EGGY_RUN_INPUT_NONE);
    assert(m.score == paused_score);
    eggy_run_step(&m, 16, EGGY_RUN_INPUT_TOGGLE_PAUSE);
    assert(m.state == EGGY_RUN_PLAYING);

    return 0;
}
