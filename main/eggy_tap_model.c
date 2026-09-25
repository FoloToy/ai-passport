// eggy_tap_model.c — pure Quick Tap FSM.
#include "eggy_tap_model.h"

static void arm_wait(eggy_tap_model_t *m) {
    m->phase = EGGY_TAP_WAIT;
    m->wait_dur_ms = eggy_rng_range(&m->rng,
                                    EGGY_TAP_WAIT_MIN_MS,
                                    EGGY_TAP_WAIT_MAX_MS);
    // UINT64_MAX = "not yet latched"; the first event() tick sets it to now.
    m->wait_start_us = UINT64_MAX;
    m->go_us = 0;
}

void eggy_tap_reset(eggy_tap_model_t *m, uint32_t rng_seed, uint64_t now_us) {
    (void)now_us;
    eggy_rng_seed(&m->rng, rng_seed);
    m->round = 0;
    m->fouls = 0;
    for (int i = 0; i < EGGY_TAP_ROUNDS; i++) {
        m->reactions_ms[i] = 0;
    }
    arm_wait(m);
}

// Side effect only: bump round, arm next WAIT, or finish. The caller returns
// the round event (RECORDED/FOULED/MISSED) and inspects m->phase afterwards.
static void advance_round(eggy_tap_model_t *m) {
    m->round++;
    if (m->round >= EGGY_TAP_ROUNDS) {
        m->phase = EGGY_TAP_DONE;
    } else {
        arm_wait(m);
    }
}

eggy_tap_result_t eggy_tap_event(eggy_tap_model_t *m, uint64_t now_us,
                                 eggy_tap_input_t input) {
    eggy_tap_result_t res = { EGGY_TAP_EVENT_NONE, 0 };
    if (m->phase == EGGY_TAP_DONE) {
        return res;
    }

    if (m->phase == EGGY_TAP_WAIT) {
        if (m->wait_start_us == UINT64_MAX) {
            m->wait_start_us = now_us;
        }
        uint64_t elapsed_ms = (now_us - m->wait_start_us) / 1000ULL;
        if (input == EGGY_TAP_INPUT_OK) {
            m->reactions_ms[m->round] = EGGY_TAP_FOUL_MS;
            m->fouls++;
            advance_round(m);
            res.kind = EGGY_TAP_EVENT_FOULED;
            res.arg_ms = EGGY_TAP_FOUL_MS;
            return res;
        }
        if (elapsed_ms >= m->wait_dur_ms) {
            m->phase = EGGY_TAP_GO;
            m->go_us = now_us;
            res.kind = EGGY_TAP_EVENT_GO_NOW;
            return res;
        }
        return res;
    }

    if (m->phase == EGGY_TAP_GO) {
        if (input == EGGY_TAP_INPUT_OK) {
            uint16_t rt = (uint16_t)((now_us - m->go_us) / 1000ULL);
            m->reactions_ms[m->round] = rt;
            advance_round(m);
            res.kind = EGGY_TAP_EVENT_RECORDED;
            res.arg_ms = rt;
            return res;
        }
        uint64_t elapsed_ms = (now_us - m->go_us) / 1000ULL;
        if (elapsed_ms >= EGGY_TAP_GO_TIMEOUT_MS) {
            m->reactions_ms[m->round] = EGGY_TAP_MISS_MS;
            advance_round(m);
            res.kind = EGGY_TAP_EVENT_MISSED;
            res.arg_ms = EGGY_TAP_MISS_MS;
            return res;
        }
    }
    return res;
}

uint32_t eggy_tap_average_ms(const eggy_tap_model_t *m) {
    uint32_t sum = 0;
    for (int i = 0; i < EGGY_TAP_ROUNDS; i++) {
        sum += m->reactions_ms[i];
    }
    return sum / EGGY_TAP_ROUNDS;
}
