// eggy_run_model.h — pure game logic for the "Eggy Run" side-scroller.
// No ESP-IDF/LVGL dependency; host-testable. Coordinates are screen pixels
// on the 240x320 playfield (the view maps 1:1).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "eggy_rng.h"

// ---- Playfield geometry (px) ----
#define EGGY_RUN_W        240
#define EGGY_RUN_H        320
#define EGGY_GROUND_Y     260   // top of the ground band; eggy rests with bottom here
#define EGGY_X            48
#define EGGY_W            24
#define EGGY_STAND_H      24
#define EGGY_DUCK_H       14
#define EGGY_OBSTACLE_W   16
#define EGGY_SPIKE_H      22    // ground spike: jump over
#define EGGY_BAR_TOP      218   // overhead bar: duck under (bar spans 218..246)
#define EGGY_BAR_H        28

// ---- Physics (px, s) ----
#define EGGY_JUMP_VY      (-360)   // initial jump velocity (up negative)
#define EGGY_GRAVITY      1100     // px/s^2
#define EGGY_DUCK_MS      420      // how long a duck lasts from one DOWN press

// ---- Difficulty ----
#define EGGY_SPEED_BASE   120      // px/s at score 0
#define EGGY_SPEED_MAX    260      // px/s cap
#define EGGY_SPEED_PER_SCORE 4     // px/s added per 100 score points
#define EGGY_SPAWN_MIN_MS 700      // min gap between obstacles
#define EGGY_SPAWN_MAX_MS 1400     // max gap between obstacles
#define EGGY_MAX_OBSTACLES 8

typedef enum {
    EGGY_OBSTACLE_NONE = 0,
    EGGY_OBSTACLE_SPIKE,   // ground: jump
    EGGY_OBSTACLE_BAR,     // overhead: duck
} eggy_obstacle_kind_t;

typedef struct {
    eggy_obstacle_kind_t kind;
    int16_t x;            // left edge; moves left over time
    bool active;
    bool passed;          // already scored/cleared
} eggy_obstacle_t;

typedef enum {
    EGGY_RUN_READY = 0,   // armed, waiting for first input to start running
    EGGY_RUN_PLAYING,
    EGGY_RUN_PAUSED,
    EGGY_RUN_GAME_OVER,
} eggy_run_state_t;

typedef enum {
    EGGY_RUN_INPUT_NONE = 0,
    EGGY_RUN_INPUT_JUMP,        // UP press
    EGGY_RUN_INPUT_DUCK,        // DOWN press (timed duck)
    EGGY_RUN_INPUT_TOGGLE_PAUSE,// OK click
    EGGY_RUN_INPUT_START,       // OK click from READY
    EGGY_RUN_INPUT_RESTART,     // OK click from GAME_OVER
} eggy_run_input_t;

typedef enum {
    EGGY_RUN_EVENT_NONE = 0,
    EGGY_RUN_EVENT_JUMPED,
    EGGY_RUN_EVENT_DUCKED,
    EGGY_RUN_EVENT_CRASHED,
    EGGY_RUN_EVENT_PASSED,   // cleared an obstacle
} eggy_run_event_t;

typedef struct {
    eggy_run_state_t state;
    int16_t eggy_y;          // eggy BOTTOM edge; rest = EGGY_GROUND_Y
    int16_t eggy_vy;         // px/s (negative = rising)
    int16_t duck_ms;         // >0 while ducking
    eggy_obstacle_t obstacles[EGGY_MAX_OBSTACLES];
    int32_t score;           // 1 per ~px of distance; view shows score/10
    uint32_t speed_pxps;     // current scroll speed
    uint32_t spawn_timer_ms;// counts down to next spawn
    uint32_t elapsed_ms;
    eggy_rng_t rng;
} eggy_run_model_t;

// Reset to READY at the start of a run (keeps the rng seeded by caller).
void eggy_run_reset(eggy_run_model_t *m);

// Advance one tick. dt_ms is the elapsed real time since the last step (the
// view clamps it). Returns an event (e.g. CRASHED, PASSED) for SFX.
eggy_run_event_t eggy_run_step(eggy_run_model_t *m, uint32_t dt_ms,
                               eggy_run_input_t input);

// Current eggy height (for drawing): stand or duck.
static inline int eggy_run_eggy_h(const eggy_run_model_t *m) {
    return m->duck_ms > 0 ? EGGY_DUCK_H : EGGY_STAND_H;
}

// Eggy TOP edge = bottom - height (ducking lowers the top, bottom stays put).
static inline int eggy_run_eggy_top(const eggy_run_model_t *m) {
    return (int)m->eggy_y - eggy_run_eggy_h(m);
}
