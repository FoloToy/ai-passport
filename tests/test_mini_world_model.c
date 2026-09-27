// tests/test_mini_world_model.c — pure tile-sandbox logic host test.
#include <assert.h>
#include <stdint.h>
#include <math.h>

#include "mini_world_model.h"

#define FLAT_SURF 24

static int surface_at(const mini_world_model_t *m, int x) {
    for (int y = 0; y < MINI_WORLD_H; y++) {
        if (m->grid[y][x] != MINI_AIR) return y;
    }
    return MINI_WORLD_H;
}

// Flat world: ground row FLAT_SURF = GRASS, below = DIRT/STONE, rest AIR.
static void flat_world(mini_world_model_t *m) {
    mini_rng_seed(&m->rng, 1);
    for (int y = 0; y < MINI_WORLD_H; y++) {
        for (int x = 0; x < MINI_WORLD_W; x++) m->grid[y][x] = MINI_AIR;
    }
    for (int x = 0; x < MINI_WORLD_W; x++) {
        m->grid[FLAT_SURF][x] = MINI_GRASS;
        for (int y = FLAT_SURF + 1; y < MINI_WORLD_H; y++) {
            m->grid[y][x] = (y < FLAT_SURF + 5) ? MINI_DIRT : MINI_STONE;
        }
    }
    m->px = 2.0f;
    m->py = (float)FLAT_SURF - MINI_PH;
    m->vx = 0.0f;
    m->vy = 0.0f;
    m->facing = MINI_FACING_RIGHT;
    m->on_ground = true;
    m->digs = 0;
}

int main(void) {
    mini_world_model_t m, m2;

    // Deterministic generation.
    mini_world_generate(&m, 42);
    mini_world_generate(&m2, 42);
    for (int y = 0; y < MINI_WORLD_H; y++) {
        for (int x = 0; x < MINI_WORLD_W; x++) {
            assert(m.grid[y][x] == m2.grid[y][x]);
        }
    }

    // Terrain layering: topmost solid is GRASS, dirt below, stone at the bottom.
    for (int x = 0; x < MINI_WORLD_W; x++) {
        int surf = surface_at(&m, x);
        assert(surf >= 0 && surf < MINI_WORLD_H);
        assert(m.grid[surf][x] == MINI_GRASS);
        assert(m.grid[surf + 1][x] == MINI_DIRT);
        assert(m.grid[MINI_WORLD_H - 1][x] == MINI_STONE);
    }

    // Player starts on the surface at column 2.
    {
        int surf2 = surface_at(&m, 2);
        assert(m.px == 2.0f);
        assert(m.py == (float)surf2 - MINI_PH);
        assert(m.on_ground);
    }

    // Gravity: lift the player, it falls back and lands on the ground.
    flat_world(&m);
    m.py -= 5.0f;
    m.on_ground = false;
    for (int i = 0; i < 300; i++) mini_world_step(&m, 16, false, false, false);
    assert(m.on_ground);
    assert(m.py + MINI_PH <= (float)FLAT_SURF + 0.001f);  // not through ground
    assert(m.py + MINI_PH > (float)FLAT_SURF - 0.5f);      // resting on it

    // Walk right moves the player; a body-height wall blocks it.
    flat_world(&m);
    float px0 = m.px;
    for (int i = 0; i < 60; i++) mini_world_step(&m, 16, false, true, false);
    assert(m.px > px0);
    // Place a wall at body height a few tiles ahead and walk into it.
    flat_world(&m);
    m.grid[FLAT_SURF - 1][6] = MINI_STONE;  // body-height wall at col 6
    for (int i = 0; i < 200; i++) mini_world_step(&m, 16, false, true, false);
    assert(m.px < 5.5f);   // blocked before the wall at col 6
    assert(m.px > 2.0f);   // did walk forward

    // Jump only when grounded; returns to ground.
    flat_world(&m);
    float pj = m.py;
    mini_world_step(&m, 16, false, false, true);
    assert(m.vy < 0.0f);   // jumped, moving up (gravity already applied this tick)
    for (int i = 0; i < 250; i++) mini_world_step(&m, 16, false, false, false);
    assert(m.on_ground);
    assert(m.py > pj - 0.1f && m.py < pj + 0.01f);  // landed back near rest height

    // Jump request in the air does nothing (no double jump).
    flat_world(&m);
    m.py -= 3.0f;
    m.on_ground = false;
    m.vy = 0.0f;
    mini_world_step(&m, 16, false, false, true);
    assert(m.vy >= 0.0f);   // no upward jump, just gravity

    // Break: place a solid block in front and break it.
    flat_world(&m);
    int mid_tx = (int)floorf(m.px + MINI_PW * 0.5f);
    int ftx = mid_tx + m.facing;
    int fty = (int)floorf(m.py + MINI_PH * 0.5f);
    m.grid[fty][ftx] = MINI_DIRT;
    assert(mini_world_break(&m));
    assert(m.grid[fty][ftx] == MINI_AIR);
    assert(m.digs == 1);
    assert(!mini_world_break(&m));   // already air

    // Place: front air becomes dirt; placing into solid fails.
    assert(mini_world_place(&m));
    assert(m.grid[fty][ftx] == MINI_DIRT);
    assert(!mini_world_place(&m));   // solid now

    // AABB collision query.
    flat_world(&m);
    assert(mini_world_aabb_solid(&m, m.px, m.py + 0.01f));   // ground below
    assert(!mini_world_aabb_solid(&m, m.px, m.py - 1.0f));    // open air above

    return 0;
}
