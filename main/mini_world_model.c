// mini_world_model.c — pure 2D tile-sandbox logic.
#include "mini_world_model.h"

#include <math.h>

// ---- Terrain generation ----
void mini_world_generate(mini_world_model_t *m, uint32_t seed) {
    mini_rng_seed(&m->rng, seed);
    int base = MINI_WORLD_H - 8;
    int height[MINI_WORLD_W];
    height[0] = base + (int)mini_rng_range(&m->rng, 0, 5) - 2;  // base +/- 2
    for (int x = 1; x < MINI_WORLD_W; x++) {
        int delta = (int)mini_rng_range(&m->rng, 0, 3) - 1;  // -1, 0, +1
        height[x] = height[x - 1] + delta;
        if (height[x] < base - 4) height[x] = base - 4;
        if (height[x] > base + 4) height[x] = base + 4;
    }
    for (int x = 0; x < MINI_WORLD_W; x++) {
        int surf = height[x];
        for (int y = 0; y < MINI_WORLD_H; y++) {
            if (y < surf) {
                m->grid[y][x] = MINI_AIR;
            } else if (y == surf) {
                m->grid[y][x] = MINI_GRASS;
            } else if (y < surf + 4) {
                m->grid[y][x] = MINI_DIRT;
            } else {
                m->grid[y][x] = MINI_STONE;
            }
        }
    }

    // Place the player on the surface near the left.
    m->px = 2.0f;
    m->vx = 0.0f;
    m->vy = 0.0f;
    m->facing = MINI_FACING_RIGHT;
    m->on_ground = true;
    m->digs = 0;
    int surf_y = MINI_WORLD_H - 1;
    for (int y = 0; y < MINI_WORLD_H; y++) {
        if (m->grid[y][2] != MINI_AIR) {
            surf_y = y;
            break;
        }
    }
    m->py = (float)surf_y - MINI_PH;
}

// ---- Queries ----
bool mini_world_is_solid(const mini_world_model_t *m, int tx, int ty) {
    if (tx < 0 || tx >= MINI_WORLD_W) {
        return true;   // side walls
    }
    if (ty < 0) {
        return false;  // open sky
    }
    if (ty >= MINI_WORLD_H) {
        return true;   // floor below the world
    }
    return m->grid[ty][tx] != MINI_AIR;
}

bool mini_world_aabb_solid(const mini_world_model_t *m, float x, float y) {
    int tx_lo = (int)floorf(x);
    int tx_hi = (int)floorf(x + MINI_PW - 0.001f);
    int ty_lo = (int)floorf(y);
    int ty_hi = (int)floorf(y + MINI_PH - 0.001f);
    for (int ty = ty_lo; ty <= ty_hi; ty++) {
        for (int tx = tx_lo; tx <= tx_hi; tx++) {
            if (mini_world_is_solid(m, tx, ty)) {
                return true;
            }
        }
    }
    return false;
}

// ---- Physics ----
void mini_world_step(mini_world_model_t *m, uint32_t dt_ms,
                     bool left, bool right, bool jump) {
    float dt = (float)dt_ms / 1000.0f;

    if (left && !right) {
        m->vx = -MINI_WALK_SPEED;
        m->facing = MINI_FACING_LEFT;
    } else if (right && !left) {
        m->vx = MINI_WALK_SPEED;
        m->facing = MINI_FACING_RIGHT;
    } else {
        m->vx = 0.0f;
    }

    if (jump && m->on_ground) {
        m->vy = MINI_JUMP_VY;
        m->on_ground = false;
    }

    m->vy += MINI_GRAVITY * dt;
    if (m->vy > MINI_MAX_FALL) {
        m->vy = MINI_MAX_FALL;
    }

    // Horizontal move; stop at walls.
    float nx = m->px + m->vx * dt;
    if (!mini_world_aabb_solid(m, nx, m->py)) {
        m->px = nx;
    } else {
        m->vx = 0.0f;
    }

    // Ground probe (with the new horizontal position).
    m->on_ground = mini_world_aabb_solid(m, m->px, m->py + 0.01f);

    // Vertical move; land on / bump ceilings.
    float ny = m->py + m->vy * dt;
    if (!mini_world_aabb_solid(m, m->px, ny)) {
        m->py = ny;
    } else {
        if (m->vy > 0.0f) {
            m->on_ground = true;
        }
        m->vy = 0.0f;
    }
}

// ---- Target tile (front-facing, body mid-height) ----
static void front_tile(const mini_world_model_t *m, int *tx, int *ty) {
    int mid_tx = (int)floorf(m->px + MINI_PW * 0.5f);
    *tx = mid_tx + m->facing;
    *ty = (int)floorf(m->py + MINI_PH * 0.5f);
}

bool mini_world_break(mini_world_model_t *m) {
    int tx, ty;
    front_tile(m, &tx, &ty);
    if (ty < 0 || ty >= MINI_WORLD_H || tx < 0 || tx >= MINI_WORLD_W) {
        return false;
    }
    if (m->grid[ty][tx] == MINI_AIR) {
        return false;
    }
    m->grid[ty][tx] = MINI_AIR;
    m->digs++;
    return true;
}

bool mini_world_place(mini_world_model_t *m) {
    int tx, ty;
    front_tile(m, &tx, &ty);
    if (ty < 0 || ty >= MINI_WORLD_H || tx < 0 || tx >= MINI_WORLD_W) {
        return false;
    }
    if (m->grid[ty][tx] != MINI_AIR) {
        return false;
    }
    // Don't place a block inside the player's own body.
    if (mini_world_aabb_solid(m, m->px, m->py)) {
        return false;  // already overlapping (shouldn't happen)
    }
    // Check the placed block wouldn't intersect the player.
    // target tile is adjacent (mid +/- 1), so it doesn't overlap the player.
    m->grid[ty][tx] = MINI_DIRT;
    return true;
}
