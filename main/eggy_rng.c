// eggy_rng.c — xorshift32 implementation.
#include "eggy_rng.h"

void eggy_rng_seed(eggy_rng_t *r, uint32_t seed) {
    r->state = (seed == 0) ? 0xA5A5A5A5u : seed;
}

uint32_t eggy_rng_next(eggy_rng_t *r) {
    uint32_t x = r->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->state = x;
    return x;
}

uint32_t eggy_rng_range(eggy_rng_t *r, uint32_t lo, uint32_t hi) {
    if (hi <= lo) {
        return lo;
    }
    uint32_t span = hi - lo;
    // map a 32-bit value into [0, span) without modulo skew for small spans
    return lo + (eggy_rng_next(r) % span);
}
