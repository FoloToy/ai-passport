// eggy_spin_model.h — pure physics for the "Lucky Spin" wheel.
// No ESP-IDF/LVGL; host-testable. Angle in degrees, angular velocity in deg/s.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "eggy_rng.h"

#define EGGY_SPIN_SEGMENTS        8
#define EGGY_SPIN_KICK_MIN_DPS    540.0f
#define EGGY_SPIN_KICK_MAX_DPS    900.0f
#define EGGY_SPIN_DRAG_PER_S      1.6f   // exponential drag; vel *= e^(-drag*dt)
#define EGGY_SPIN_STOP_DPS        8.0f   // below this the wheel settles

typedef struct {
    float angle_deg;       // 0..360
    float angular_vel_dps;  // current speed (can be negative)
    bool spinning;
    int8_t result;          // segment index [0,SEGMENTS) after settling; -1 while spinning
    eggy_rng_t rng;
} eggy_spin_model_t;

void eggy_spin_reset(eggy_spin_model_t *m, uint32_t rng_seed);

// Start a spin from idle. Returns false if already spinning.
bool eggy_spin_kick(eggy_spin_model_t *m);

// Advance one tick. dt_ms is elapsed time. When the wheel settles, sets
// `result` and returns true (caller plays a "settle" cue); otherwise false.
bool eggy_spin_step(eggy_spin_model_t *m, uint32_t dt_ms);

// Segment index under the pointer for a given angle (0..SEGMENTS-1).
int eggy_spin_segment_for(float angle_deg);
