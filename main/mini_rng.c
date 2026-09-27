// mini_rng.c — xorshift32.
#include "mini_rng.h"

void mini_rng_seed(mini_rng_t *r, uint32_t seed) {
    r->state = (seed == 0) ? 0xA5A5A5A5u : seed;
}

uint32_t mini_rng_next(mini_rng_t *r) {
    uint32_t x = r->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->state = x;
    return x;
}

uint32_t mini_rng_range(mini_rng_t *r, uint32_t lo, uint32_t hi) {
    if (hi <= lo) {
        return lo;
    }
    return lo + (mini_rng_next(r) % (hi - lo));
}
