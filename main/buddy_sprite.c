#include "buddy_sprite.h"

#include <stddef.h>

enum {
    PIX_BG = 0, PIX_INK = 1, PIX_DIM = 2, PIX_LINE = 3, PIX_ORANGE = 4,
    PIX_RED = 5, PIX_GREEN = 6, PIX_YELLOW = 7, PIX_BLUE = 8, PIX_WHITE = 9,
    PIX_TERRA = 10, PIX_BROWN = 11, PIX_PURPLE = 12, PIX_SKY = 13,
    PIX_CREAM = 14, PIX_PANEL = 15,
};

typedef enum {
    SHAPE_ROUND, SHAPE_TALL, SHAPE_WIDE, SHAPE_SHELL, SHAPE_GHOST,
    SHAPE_PLANT, SHAPE_ROBOT, SHAPE_BIRD, SHAPE_TENTACLE,
} shape_t;

typedef enum {
    DEC_EARS, DEC_BILL, DEC_NECK, DEC_HORNS, DEC_WINGS, DEC_OWL,
    DEC_FLIPPERS, DEC_SHELL, DEC_SNAIL, DEC_GHOST, DEC_GILLS, DEC_CACTUS,
    DEC_ANTENNA, DEC_RABBIT, DEC_MUSHROOM, DEC_CHONK, DEC_NONE,
} decoration_t;

typedef struct {
    const char *name;
    uint8_t body;
    uint8_t accent;
    shape_t shape;
    decoration_t decoration;
} species_t;

static const species_t s_species[BUDDY_SPRITE_SPECIES_COUNT] = {
    {"CAPYBARA", PIX_TERRA, PIX_BROWN, SHAPE_WIDE, DEC_EARS},
    {"DUCK", PIX_YELLOW, PIX_ORANGE, SHAPE_BIRD, DEC_BILL},
    {"GOOSE", PIX_WHITE, PIX_ORANGE, SHAPE_TALL, DEC_NECK},
    {"BLOB", PIX_PURPLE, PIX_SKY, SHAPE_ROUND, DEC_NONE},
    {"CAT", PIX_TERRA, PIX_CREAM, SHAPE_ROUND, DEC_EARS},
    {"DRAGON", PIX_GREEN, PIX_YELLOW, SHAPE_TALL, DEC_HORNS},
    {"OCTOPUS", PIX_RED, PIX_CREAM, SHAPE_TENTACLE, DEC_NONE},
    {"OWL", PIX_BROWN, PIX_CREAM, SHAPE_BIRD, DEC_OWL},
    {"PENGUIN", PIX_INK, PIX_WHITE, SHAPE_TALL, DEC_FLIPPERS},
    {"TURTLE", PIX_GREEN, PIX_YELLOW, SHAPE_SHELL, DEC_SHELL},
    {"SNAIL", PIX_PURPLE, PIX_YELLOW, SHAPE_SHELL, DEC_SNAIL},
    {"GHOST", PIX_WHITE, PIX_SKY, SHAPE_GHOST, DEC_GHOST},
    {"AXOLOTL", PIX_SKY, PIX_PURPLE, SHAPE_WIDE, DEC_GILLS},
    {"CACTUS", PIX_GREEN, PIX_YELLOW, SHAPE_PLANT, DEC_CACTUS},
    {"ROBOT", PIX_DIM, PIX_BLUE, SHAPE_ROBOT, DEC_ANTENNA},
    {"RABBIT", PIX_CREAM, PIX_TERRA, SHAPE_TALL, DEC_RABBIT},
    {"MUSHROOM", PIX_RED, PIX_CREAM, SHAPE_PLANT, DEC_MUSHROOM},
    {"CHONK", PIX_TERRA, PIX_CREAM, SHAPE_WIDE, DEC_CHONK},
};

static void rect(buddy_i4_surface_t *s, const buddy_i4_clip_t *c,
                 int x, int y, int w, int h, uint8_t color)
{
    buddy_i4_fill_rect(s, c, x, y, w, h, color);
}

static void eye(buddy_i4_surface_t *s, const buddy_i4_clip_t *c,
                int x, int y, bool closed)
{
    rect(s, c, x, y + (closed ? 2 : 0), 5, closed ? 2 : 5, PIX_INK);
    if (!closed) rect(s, c, x + 1, y, 1, 1, PIX_WHITE);
}

static void heart(buddy_i4_surface_t *s, const buddy_i4_clip_t *c, int x, int y)
{
    rect(s, c, x + 2, y, 3, 3, PIX_RED); rect(s, c, x + 7, y, 3, 3, PIX_RED);
    rect(s, c, x, y + 2, 12, 5, PIX_RED); rect(s, c, x + 2, y + 7, 8, 3, PIX_RED);
    rect(s, c, x + 4, y + 10, 4, 2, PIX_RED);
}

static void decorations(buddy_i4_surface_t *s, const buddy_i4_clip_t *c,
                        const species_t *pet, int x, int y)
{
    switch (pet->decoration) {
    case DEC_EARS:
        rect(s, c, x + 12, y + 5, 8, 9, pet->body); rect(s, c, x + 40, y + 5, 8, 9, pet->body); break;
    case DEC_BILL:
        rect(s, c, x + 45, y + 27, 12, 6, pet->accent); break;
    case DEC_NECK:
        rect(s, c, x + 35, y + 15, 11, 26, pet->body); rect(s, c, x + 42, y + 15, 10, 7, pet->accent); break;
    case DEC_HORNS:
        rect(s, c, x + 16, y + 3, 5, 10, pet->accent); rect(s, c, x + 40, y + 3, 5, 10, pet->accent); break;
    case DEC_OWL:
        rect(s, c, x + 10, y + 16, 16, 16, pet->accent); rect(s, c, x + 34, y + 16, 16, 16, pet->accent); break;
    case DEC_FLIPPERS:
        rect(s, c, x + 5, y + 27, 8, 22, pet->body); rect(s, c, x + 47, y + 27, 8, 22, pet->body); break;
    case DEC_SHELL:
        rect(s, c, x + 14, y + 21, 34, 25, pet->accent); rect(s, c, x + 20, y + 26, 22, 15, pet->body); break;
    case DEC_SNAIL:
        rect(s, c, x + 8, y + 24, 34, 27, pet->accent); rect(s, c, x + 17, y + 31, 16, 13, pet->body); break;
    case DEC_GHOST:
        rect(s, c, x + 14, y + 48, 8, 8, PIX_BG); rect(s, c, x + 38, y + 48, 8, 8, PIX_BG); break;
    case DEC_GILLS:
        rect(s, c, x + 6, y + 15, 9, 5, pet->accent); rect(s, c, x + 3, y + 23, 12, 5, pet->accent);
        rect(s, c, x + 45, y + 15, 9, 5, pet->accent); rect(s, c, x + 45, y + 23, 12, 5, pet->accent); break;
    case DEC_CACTUS:
        rect(s, c, x + 5, y + 27, 12, 7, pet->body); rect(s, c, x + 7, y + 19, 6, 15, pet->body);
        rect(s, c, x + 43, y + 22, 12, 7, pet->body); rect(s, c, x + 47, y + 15, 6, 14, pet->body); break;
    case DEC_ANTENNA:
        rect(s, c, x + 29, y + 2, 3, 10, pet->accent); rect(s, c, x + 26, y, 9, 5, pet->accent); break;
    case DEC_RABBIT:
        rect(s, c, x + 14, y, 10, 23, pet->body); rect(s, c, x + 37, y, 10, 23, pet->body);
        rect(s, c, x + 17, y + 4, 4, 15, pet->accent); rect(s, c, x + 40, y + 4, 4, 15, pet->accent); break;
    case DEC_MUSHROOM:
        rect(s, c, x + 5, y + 8, 50, 16, pet->body); rect(s, c, x + 13, y + 3, 34, 6, pet->body);
        rect(s, c, x + 17, y + 10, 7, 5, PIX_CREAM); rect(s, c, x + 39, y + 15, 6, 5, PIX_CREAM); break;
    case DEC_CHONK:
        rect(s, c, x + 4, y + 30, 52, 23, pet->body); break;
    default: break;
    }
}

const char *buddy_sprite_name(uint8_t species)
{
    return s_species[species % BUDDY_SPRITE_SPECIES_COUNT].name;
}

bool buddy_sprite_bounds(uint8_t species, uint8_t state, uint32_t tick,
                         buddy_sprite_bounds_t *bounds)
{
    (void)species; (void)state; (void)tick;
    if (bounds == NULL) return false;
    bounds->x = 2; bounds->y = 0; bounds->w = 56; bounds->h = 60;
    return true;
}

void buddy_sprite_render(buddy_i4_surface_t *surface, const buddy_i4_clip_t *clip,
                         uint8_t species, uint8_t state, uint32_t tick,
                         int origin_x, int origin_y)
{
    const species_t *pet = &s_species[species % BUDDY_SPRITE_SPECIES_COUNT];
    bool closed = state == 0 || state == 5 || (state == 1 && tick % 19U == 0);
    int bounce = state == 4 ? -(int)(tick % 4U < 2U ? 3 : 0) : 0;
    int breathe = (state == 0 && tick % 6U >= 3U) ? 1 : 0;
    int x = origin_x + 2;
    int y = origin_y + 6 + bounce;
    int body_x = x + 8;
    int body_y = y + 12;
    int body_w = 44;
    int body_h = 40 + breathe;

    if (surface == NULL) return;
    if (pet->shape == SHAPE_WIDE || pet->shape == SHAPE_SHELL) { body_x = x + 4; body_y += 8; body_w = 52; body_h = 31; }
    else if (pet->shape == SHAPE_TALL || pet->shape == SHAPE_PLANT || pet->shape == SHAPE_ROBOT) { body_x = x + 11; body_w = 38; body_h = 45; }
    else if (pet->shape == SHAPE_TENTACLE) { body_y += 2; body_h = 35; }

    rect(surface, clip, body_x + 4, body_y - 4, body_w - 8, body_h + 7, pet->body);
    rect(surface, clip, body_x, body_y + 3, body_w, body_h - 8, pet->body);
    decorations(surface, clip, pet, x, y);
    if (pet->shape == SHAPE_TENTACLE) {
        rect(surface, clip, x + 7, y + 43, 8, 14, pet->body); rect(surface, clip, x + 20, y + 44, 8, 16, pet->body);
        rect(surface, clip, x + 34, y + 44, 8, 16, pet->body); rect(surface, clip, x + 47, y + 43, 8, 14, pet->body);
    }
    eye(surface, clip, x + 18, y + 24, closed);
    eye(surface, clip, x + 37, y + 24, closed);
    if (state == 2) {
        rect(surface, clip, x + 22 + (int)(tick % 3U), y + 39, 17, 3, pet->accent);
    } else if (state == 3) {
        rect(surface, clip, x + 27, y + 39, 8, 5, PIX_INK);
    } else {
        rect(surface, clip, x + 25, y + 39, 12, 2, PIX_INK);
    }
    if (state == 4) {
        rect(surface, clip, origin_x + 2, origin_y + 4, 3, 3, PIX_YELLOW);
        rect(surface, clip, origin_x + 58, origin_y + 13, 3, 3, PIX_GREEN);
        rect(surface, clip, origin_x + 8, origin_y + 52, 3, 3, PIX_RED);
    } else if (state == 5) {
        rect(surface, clip, x + 16, y + 23, 7, 2, PIX_INK); rect(surface, clip, x + 18, y + 21, 2, 7, PIX_INK);
        rect(surface, clip, x + 35, y + 23, 7, 2, PIX_INK); rect(surface, clip, x + 37, y + 21, 2, 7, PIX_INK);
    } else if (state == 6) {
        heart(surface, clip, origin_x + 46, origin_y + 1);
    }
}
