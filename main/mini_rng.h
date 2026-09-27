// mini_rng.h — deterministic RNG for mini-world (terrain + game).
// Pure C (no ESP-IDF/LVGL) so the model stays host-testable.
#pragma once

#include <stdint.h>

typedef struct {
    uint32_t state;
} mini_rng_t;

void mini_rng_seed(mini_rng_t *r, uint32_t seed);
uint32_t mini_rng_next(mini_rng_t *r);
uint32_t mini_rng_range(mini_rng_t *r, uint32_t lo, uint32_t hi);
