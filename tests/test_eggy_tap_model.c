// tests/test_eggy_tap_model.c — pure Quick Tap FSM host test.
#include <assert.h>
#include <stdint.h>

#include "eggy_tap_model.h"

int main(void) {
    eggy_tap_model_t m;

    // Fresh round: WAIT with a randomized wait window. now in microseconds.
    eggy_tap_reset(&m, 7, 0);
    assert(m.phase == EGGY_TAP_WAIT);
    assert(m.round == 0);
    assert(m.wait_start_us == UINT64_MAX);   // not yet latched

    // First tick latches wait_start to now.
    eggy_tap_result_t r = eggy_tap_event(&m, 1000, EGGY_TAP_INPUT_NONE);
    assert(r.kind == EGGY_TAP_EVENT_NONE);
    assert(m.wait_start_us == 1000);

    // Pressing during WAIT is a foul.
    uint64_t now = 1000 + (m.wait_dur_ms / 2) * 1000ULL;
    r = eggy_tap_event(&m, now, EGGY_TAP_INPUT_OK);
    assert(r.kind == EGGY_TAP_EVENT_FOULED);
    assert(m.reactions_ms[0] == EGGY_TAP_FOUL_MS);
    assert(m.round == 1);

    // Drive to the GO transition on a clean round.
    eggy_tap_reset(&m, 7, 0);
    eggy_tap_event(&m, 0, EGGY_TAP_INPUT_NONE);          // latch start=0
    now = (uint64_t)(m.wait_dur_ms + 10) * 1000ULL;
    r = eggy_tap_event(&m, now, EGGY_TAP_INPUT_NONE);
    assert(r.kind == EGGY_TAP_EVENT_GO_NOW);
    assert(m.phase == EGGY_TAP_GO);
    assert(m.go_us == now);

    // Fast press records a small reaction and advances the round.
    uint64_t press = now + 180 * 1000ULL;  // 180 ms
    r = eggy_tap_event(&m, press, EGGY_TAP_INPUT_OK);
    assert(r.kind == EGGY_TAP_EVENT_RECORDED);
    assert(r.arg_ms == 180);
    assert(m.reactions_ms[0] == 180);
    assert(m.round == 1);
    assert(m.phase == EGGY_TAP_WAIT);

    // GO timeout records a miss.
    eggy_tap_reset(&m, 99, 0);
    eggy_tap_event(&m, 0, EGGY_TAP_INPUT_NONE);
    now = (uint64_t)(m.wait_dur_ms + 10) * 1000ULL;
    eggy_tap_event(&m, now, EGGY_TAP_INPUT_NONE);   // -> GO
    now += (EGGY_TAP_GO_TIMEOUT_MS + 10) * 1000ULL;
    r = eggy_tap_event(&m, now, EGGY_TAP_INPUT_NONE);
    assert(r.kind == EGGY_TAP_EVENT_MISSED);
    assert(m.reactions_ms[0] == EGGY_TAP_MISS_MS);

    // Completing all rounds reaches DONE; average is sensible.
    eggy_tap_reset(&m, 5, 0);
    for (uint8_t i = 0; i < EGGY_TAP_ROUNDS; i++) {
        eggy_tap_event(&m, (uint64_t)i * 1000000ULL, EGGY_TAP_INPUT_NONE);
        assert(m.wait_start_us == (uint64_t)i * 1000000ULL);
        uint64_t go = (uint64_t)i * 1000000ULL +
                      (uint64_t)(m.wait_dur_ms + 1) * 1000ULL;
        eggy_tap_event(&m, go, EGGY_TAP_INPUT_NONE);
        assert(m.phase == EGGY_TAP_GO);
        eggy_tap_event(&m, go + 200 * 1000ULL, EGGY_TAP_INPUT_OK);
    }
    assert(m.phase == EGGY_TAP_DONE);
    uint32_t avg = eggy_tap_average_ms(&m);
    assert(avg == 200);

    return 0;
}
