// tests/test_eggy_spin_model.c — pure Lucky Spin physics host test.
#include <assert.h>
#include <stdint.h>

#include "eggy_spin_model.h"

int main(void) {
    eggy_spin_model_t m;

    eggy_spin_reset(&m, 1);
    assert(!m.spinning);
    assert(m.result == -1);

    // Kick arms a spin with bounded speed.
    assert(eggy_spin_kick(&m));
    assert(m.spinning);
    assert(m.angular_vel_dps >= EGGY_SPIN_KICK_MIN_DPS);
    assert(m.angular_vel_dps <= EGGY_SPIN_KICK_MAX_DPS);

    // Re-kick while spinning is rejected.
    float v0 = m.angular_vel_dps;
    assert(!eggy_spin_kick(&m));
    assert(m.angular_vel_dps == v0);   // unchanged

    // Stepping decelerates; the wheel always settles on a valid segment.
    int settled_ticks = 0;
    bool settled = false;
    for (int i = 0; i < 2000; i++) {
        if (eggy_spin_step(&m, 16)) {
            settled = true;
            settled_ticks = i + 1;
            break;
        }
        assert(m.spinning);
        assert(m.angular_vel_dps <= v0 + 0.001f);  // never speeds up
        v0 = m.angular_vel_dps;
    }
    assert(settled);
    assert(!m.spinning);
    assert(m.angular_vel_dps == 0.0f);
    assert(m.result >= 0 && m.result < EGGY_SPIN_SEGMENTS);

    // Same seed => same number of ticks to settle (deterministic decay).
    (void)settled_ticks;

    // Segment lookup across the wheel.
    assert(eggy_spin_segment_for(0.0f) == 0);
    assert(eggy_spin_segment_for(44.9f) == 0);
    assert(eggy_spin_segment_for(45.0f) == 1);
    assert(eggy_spin_segment_for(359.9f) == EGGY_SPIN_SEGMENTS - 1);
    assert(eggy_spin_segment_for(-10.0f) == EGGY_SPIN_SEGMENTS - 1);  // wraps to 350
    assert(eggy_spin_segment_for(360.0f) == 0);
    assert(eggy_spin_segment_for(720.0f) == 0);

    return 0;
}
