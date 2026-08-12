#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "buddy_i4.h"

#define BUDDY_SPRITE_SPECIES_COUNT 18
#define BUDDY_SPRITE_STATE_COUNT 7

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} buddy_sprite_bounds_t;

const char *buddy_sprite_name(uint8_t species);
bool buddy_sprite_bounds(uint8_t species, uint8_t state, uint32_t tick,
                         buddy_sprite_bounds_t *bounds);
void buddy_sprite_render(buddy_i4_surface_t *surface, const buddy_i4_clip_t *clip,
                         uint8_t species, uint8_t state, uint32_t tick,
                         int origin_x, int origin_y);
