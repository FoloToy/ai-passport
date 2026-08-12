#pragma once

#include <stdint.h>

typedef struct {
    uint8_t *pixels;
    uint16_t width;
    uint16_t height;
    uint16_t stride;
} buddy_i4_surface_t;

typedef struct {
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} buddy_i4_clip_t;

void buddy_i4_set_pixel(uint8_t *pixels, uint16_t width, uint16_t x, uint16_t y,
                        uint8_t color_index);
uint8_t buddy_i4_get_pixel(const uint8_t *pixels, uint16_t width, uint16_t x, uint16_t y);
void buddy_i4_surface_init(buddy_i4_surface_t *surface, uint8_t *pixels,
                           uint16_t width, uint16_t height, uint16_t stride);
void buddy_i4_fill_rect(buddy_i4_surface_t *surface, const buddy_i4_clip_t *clip,
                        int x, int y, int width, int height, uint8_t color_index);
void buddy_i4_line(buddy_i4_surface_t *surface, const buddy_i4_clip_t *clip,
                   int x1, int y1, int x2, int y2, uint8_t color_index);
