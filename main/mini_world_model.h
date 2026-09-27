// mini_world_model.h — pure 2D tile-sandbox logic (no ESP-IDF/LVGL).
// Tile units: positions/velocities in tiles, time in ms. Host-testable.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "mini_rng.h"

#define MINI_WORLD_W   64
#define MINI_WORLD_H   32

// Tile types.
#define MINI_AIR    0
#define MINI_GRASS  1
#define MINI_DIRT   2
#define MINI_STONE  3
#define MINI_WOOD   4

// Player body (tile units).
#define MINI_PW     0.6f        // width
#define MINI_PH     1.6f        // height

// Physics.
#define MINI_GRAVITY      22.0f   // tiles/s^2
#define MINI_WALK_SPEED    4.5f  // tiles/s
#define MINI_JUMP_VY      -9.0f   // tiles/s (up negative)
#define MINI_MAX_FALL     18.0f   // terminal fall speed

#define MINI_FACING_LEFT  (-1)
#define MINI_FACING_RIGHT ( 1)

typedef struct {
    uint8_t grid[MINI_WORLD_H][MINI_WORLD_W];
    float px;        // top-left x
    float py;        // top-left y
    float vx, vy;
    int facing;      // -1 or +1
    bool on_ground;
    int digs;
    mini_rng_t rng;
} mini_world_model_t;

// Fill terrain from a seed and place the player on the surface near the left.
void mini_world_generate(mini_world_model_t *m, uint32_t seed);

// Solid query (out-of-bounds x and below are solid; above the world is air).
bool mini_world_is_solid(const mini_world_model_t *m, int tx, int ty);

// True if the player AABB at (x,y) overlaps any solid tile.
bool mini_world_aabb_solid(const mini_world_model_t *m, float x, float y);

// Advance physics one tick. left/right are held-walk states; jump requests a
// jump (only fires when on_ground). Returns an event bitmask (unused for now).
void mini_world_step(mini_world_model_t *m, uint32_t dt_ms,
                     bool left, bool right, bool jump);

// Break the front-facing solid tile at body mid-height. Returns true on break.
bool mini_world_break(mini_world_model_t *m);

// Place a dirt block in the front-facing AIR tile at body mid-height.
// Returns true if placed (fails on solid / out of bounds / would trap).
bool mini_world_place(mini_world_model_t *m);
