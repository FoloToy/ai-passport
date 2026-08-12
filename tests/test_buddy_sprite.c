#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "buddy_sprite.h"

int main(void)
{
    uint8_t guarded[64 * 32 + 2];
    buddy_i4_surface_t surface;
    buddy_i4_clip_t clip = {.x = 0, .y = 0, .w = 64, .h = 64};
    unsigned species;
    unsigned state;
    guarded[0] = guarded[sizeof(guarded) - 1] = 0xa5;
    buddy_i4_surface_init(&surface, guarded + 1, 64, 64, 32);
    assert(BUDDY_SPRITE_SPECIES_COUNT == 18);
    assert(BUDDY_SPRITE_STATE_COUNT == 7);
    for (species = 0; species < BUDDY_SPRITE_SPECIES_COUNT; ++species) {
        assert(buddy_sprite_name(species)[0] != '\0');
        for (state = 0; state < BUDDY_SPRITE_STATE_COUNT; ++state) {
            buddy_sprite_bounds_t bounds;
            unsigned nonzero = 0;
            unsigned i;
            memset(guarded + 1, 0, sizeof(guarded) - 2);
            buddy_sprite_render(&surface, &clip, species, state, 3, 0, 0);
            for (i = 1; i + 1 < sizeof(guarded); ++i) nonzero += guarded[i] != 0;
            assert(nonzero > 20);
            assert(buddy_sprite_bounds(species, state, 3, &bounds));
            assert(bounds.x >= 0 && bounds.y >= 0);
            assert(bounds.w > 0 && bounds.h > 0);
            assert(bounds.x + bounds.w <= 64 && bounds.y + bounds.h <= 64);
            assert(guarded[0] == 0xa5 && guarded[sizeof(guarded) - 1] == 0xa5);
        }
    }
    return 0;
}
