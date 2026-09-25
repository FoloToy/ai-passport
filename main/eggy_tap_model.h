// eggy_tap_model.h — pure FSM for the "Quick Tap" reaction game.
// No ESP-IDF/LVGL; the clock (now_us) is injected by the caller so the FSM
// is fully host-testable with deterministic time.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "eggy_rng.h"

#define EGGY_TAP_ROUNDS        5
#define EGGY_TAP_WAIT_MIN_MS   1500
#define EGGY_TAP_WAIT_MAX_MS   4000   // exclusive upper bound
#define EGGY_TAP_GO_TIMEOUT_MS 2000   // no press in this long => miss
#define EGGY_TAP_FOUL_MS       500    // penalty for pressing during WAIT
#define EGGY_TAP_MISS_MS       2000   // recorded when GO times out

typedef enum {
    EGGY_TAP_WAIT = 0,   // green hasn't shown yet; pressing now is a foul
    EGGY_TAP_GO,         // green: press OK as fast as possible
    EGGY_TAP_DONE,
} eggy_tap_phase_t;

typedef enum {
    EGGY_TAP_INPUT_NONE = 0,
    EGGY_TAP_INPUT_OK,    // OK press
    EGGY_TAP_INPUT_QUIT,  // handled by controller, not consumed here
} eggy_tap_input_t;

typedef enum {
    EGGY_TAP_EVENT_NONE = 0,
    EGGY_TAP_EVENT_GO_NOW,    // WAIT -> GO transition (play GO cue)
    EGGY_TAP_EVENT_RECORDED,  // a reaction was measured (arg = ms)
    EGGY_TAP_EVENT_FOULED,    // pressed too early
    EGGY_TAP_EVENT_MISSED,    // GO timed out
    EGGY_TAP_EVENT_DONE,
} eggy_tap_event_t;

typedef struct {
    eggy_tap_phase_t phase;
    uint8_t round;                 // 0..EGGY_TAP_ROUNDS-1
    uint16_t reactions_ms[EGGY_TAP_ROUNDS];
    uint8_t fouls;
    uint32_t wait_dur_ms;          // randomized per round
    uint64_t wait_start_us;        // set on first tick of a WAIT
    uint64_t go_us;                // when GO began
    eggy_rng_t rng;
} eggy_tap_model_t;

typedef struct {
    eggy_tap_event_t kind;
    uint16_t arg_ms;               // reaction time for RECORDED
} eggy_tap_result_t;

// Seed the RNG and arm the first WAIT round. now_us seeds the wait window.
void eggy_tap_reset(eggy_tap_model_t *m, uint32_t rng_seed, uint64_t now_us);

// Advance the FSM one tick. now_us is the monotonic clock from the caller.
eggy_tap_result_t eggy_tap_event(eggy_tap_model_t *m, uint64_t now_us,
                                 eggy_tap_input_t input);

// Mean of recorded reactions (foul/miss counted as their penalty values).
uint32_t eggy_tap_average_ms(const eggy_tap_model_t *m);
