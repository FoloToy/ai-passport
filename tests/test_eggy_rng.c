// tests/test_eggy_rng.c — deterministic RNG host test.
#include <assert.h>
#include <stdint.h>

#include "eggy_rng.h"

int main(void) {
    eggy_rng_t a, b;
    eggy_rng_seed(&a, 0x12345678);
    eggy_rng_seed(&b, 0x12345678);
    for (int i = 0; i < 1000; i++) {
        assert(eggy_rng_next(&a) == eggy_rng_next(&b));
    }

    // Different seeds diverge.
    eggy_rng_seed(&a, 1);
    eggy_rng_seed(&b, 2);
    int diffs = 0;
    for (int i = 0; i < 100; i++) {
        if (eggy_rng_next(&a) != eggy_rng_next(&b)) {
            diffs++;
        }
    }
    assert(diffs > 90);

    // Zero seed must not lock the generator at 0.
    eggy_rng_t z;
    eggy_rng_seed(&z, 0);
    uint32_t v = eggy_rng_next(&z);
    assert(v != 0);

    // Range stays in [lo, hi).
    eggy_rng_seed(&z, 0);
    for (int i = 0; i < 10000; i++) {
        uint32_t r = eggy_rng_range(&z, 5, 10);
        assert(r >= 5 && r < 10);
    }
    return 0;
}
