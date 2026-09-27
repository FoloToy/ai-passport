// tests/test_mini_rng.c — deterministic RNG host test.
#include <assert.h>
#include <stdint.h>

#include "mini_rng.h"

int main(void) {
    mini_rng_t a, b;
    mini_rng_seed(&a, 0x12345678);
    mini_rng_seed(&b, 0x12345678);
    for (int i = 0; i < 1000; i++) {
        assert(mini_rng_next(&a) == mini_rng_next(&b));
    }

    mini_rng_seed(&a, 1);
    mini_rng_seed(&b, 2);
    int diffs = 0;
    for (int i = 0; i < 100; i++) {
        if (mini_rng_next(&a) != mini_rng_next(&b)) diffs++;
    }
    assert(diffs > 90);

    mini_rng_t z;
    mini_rng_seed(&z, 0);
    assert(mini_rng_next(&z) != 0);

    mini_rng_seed(&z, 0);
    for (int i = 0; i < 10000; i++) {
        uint32_t r = mini_rng_range(&z, 5, 10);
        assert(r >= 5 && r < 10);
    }
    return 0;
}
