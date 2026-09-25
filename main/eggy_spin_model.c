// eggy_spin_model.c — pure Lucky Spin physics.
#include <math.h>

#include "eggy_spin_model.h"

static float wrap360(float a) {
    a = fmodf(a, 360.0f);
    if (a < 0.0f) {
        a += 360.0f;
    }
    return a;
}

void eggy_spin_reset(eggy_spin_model_t *m, uint32_t rng_seed) {
    eggy_rng_seed(&m->rng, rng_seed);
    m->angle_deg = 0.0f;
    m->angular_vel_dps = 0.0f;
    m->spinning = false;
    m->result = -1;
}

bool eggy_spin_kick(eggy_spin_model_t *m) {
    if (m->spinning) {
        return false;
    }
    uint32_t r = eggy_rng_range(&m->rng, 0, 100000);
    float t = (float)r / 100000.0f;
    float v = EGGY_SPIN_KICK_MIN_DPS +
              t * (EGGY_SPIN_KICK_MAX_DPS - EGGY_SPIN_KICK_MIN_DPS);
    // Spin clockwise (positive). Direction is a visual detail; keep it positive.
    m->angular_vel_dps = v;
    m->spinning = true;
    m->result = -1;
    return true;
}

int eggy_spin_segment_for(float angle_deg) {
    float a = wrap360(angle_deg);
    float seg = 360.0f / (float)EGGY_SPIN_SEGMENTS;
    int idx = (int)floorf(a / seg);
    if (idx < 0) {
        idx = 0;
    }
    if (idx >= EGGY_SPIN_SEGMENTS) {
        idx = EGGY_SPIN_SEGMENTS - 1;
    }
    return idx;
}

bool eggy_spin_step(eggy_spin_model_t *m, uint32_t dt_ms) {
    if (!m->spinning) {
        return false;
    }
    float dt = (float)dt_ms / 1000.0f;
    // Exponential drag: vel *= e^(-drag*dt)
    m->angular_vel_dps *= expf(-EGGY_SPIN_DRAG_PER_S * dt);
    m->angle_deg = wrap360(m->angle_deg + m->angular_vel_dps * dt);

    if (m->angular_vel_dps < 0.0f) {
        m->angular_vel_dps = -m->angular_vel_dps;
    }
    if (m->angular_vel_dps <= EGGY_SPIN_STOP_DPS) {
        m->spinning = false;
        m->angular_vel_dps = 0.0f;
        m->result = (int8_t)eggy_spin_segment_for(m->angle_deg);
        return true;   // just settled
    }
    return false;
}
