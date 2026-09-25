// eggy_rng.h — deterministic RNG for eggy party games.
// Pure C (no ESP-IDF/LVGL) so game models stay host-testable. The runtime
// seeds from esp_random(); tests seed a fixed value for reproducibility.
#pragma once

#include <stdint.h>

typedef struct {
    uint32_t state;
} eggy_rng_t;

// Seed the generator. A zero seed is remapped internally so the xorshift state
// is never 0 (which would lock it at 0).
void eggy_rng_seed(eggy_rng_t *r, uint32_t seed);

// Next raw 32-bit value.
uint32_t eggy_rng_next(eggy_rng_t *r);

// Uniform integer in [lo, hi). hi must be > lo.
uint32_t eggy_rng_range(eggy_rng_t *r, uint32_t lo, uint32_t hi);
