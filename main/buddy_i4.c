#include "buddy_i4.h"

#include <stdlib.h>

void buddy_i4_set_pixel(uint8_t *pixels, uint16_t width, uint16_t x, uint16_t y,
                        uint8_t color_index)
{
    uint32_t offset = (uint32_t)y * ((width + 1U) / 2U) + x / 2U;
    uint8_t shift = (x & 1U) ? 0U : 4U;
    pixels[offset] = (uint8_t)((pixels[offset] & ~(0x0fU << shift)) |
                               ((color_index & 0x0fU) << shift));
}

uint8_t buddy_i4_get_pixel(const uint8_t *pixels, uint16_t width, uint16_t x, uint16_t y)
{
    uint32_t offset = (uint32_t)y * ((width + 1U) / 2U) + x / 2U;
    uint8_t shift = (x & 1U) ? 0U : 4U;
    return (uint8_t)((pixels[offset] >> shift) & 0x0fU);
}

void buddy_i4_surface_init(buddy_i4_surface_t *surface, uint8_t *pixels,
                           uint16_t width, uint16_t height, uint16_t stride)
{
    if (surface == NULL) return;
    surface->pixels = pixels;
    surface->width = width;
    surface->height = height;
    surface->stride = stride != 0 ? stride : (uint16_t)((width + 1U) / 2U);
}

static int maximum(int a, int b) { return a > b ? a : b; }
static int minimum(int a, int b) { return a < b ? a : b; }

static int clipped(const buddy_i4_surface_t *surface, const buddy_i4_clip_t *clip,
                   int x, int y)
{
    if (surface == NULL || surface->pixels == NULL || x < 0 || y < 0 ||
        x >= surface->width || y >= surface->height) return 0;
    return clip == NULL || (x >= clip->x && y >= clip->y &&
                            x < clip->x + clip->w && y < clip->y + clip->h);
}

static void surface_pixel(buddy_i4_surface_t *surface, const buddy_i4_clip_t *clip,
                          int x, int y, uint8_t color_index)
{
    uint32_t offset;
    uint8_t shift;
    if (!clipped(surface, clip, x, y)) return;
    offset = (uint32_t)y * surface->stride + (unsigned)x / 2U;
    shift = (x & 1) ? 0U : 4U;
    surface->pixels[offset] = (uint8_t)((surface->pixels[offset] & ~(0x0fU << shift)) |
                                       ((color_index & 0x0fU) << shift));
}

void buddy_i4_fill_rect(buddy_i4_surface_t *surface, const buddy_i4_clip_t *clip,
                        int x, int y, int width, int height, uint8_t color_index)
{
    int left;
    int top;
    int right;
    int bottom;
    int px;
    int py;
    if (surface == NULL || width <= 0 || height <= 0) return;
    left = maximum(x, 0);
    top = maximum(y, 0);
    right = minimum(x + width, surface->width);
    bottom = minimum(y + height, surface->height);
    if (clip != NULL) {
        left = maximum(left, clip->x);
        top = maximum(top, clip->y);
        right = minimum(right, clip->x + clip->w);
        bottom = minimum(bottom, clip->y + clip->h);
    }
    for (py = top; py < bottom; ++py)
        for (px = left; px < right; ++px) surface_pixel(surface, NULL, px, py, color_index);
}

void buddy_i4_line(buddy_i4_surface_t *surface, const buddy_i4_clip_t *clip,
                   int x1, int y1, int x2, int y2, uint8_t color_index)
{
    int dx = abs(x2 - x1);
    int sx = x1 < x2 ? 1 : -1;
    int dy = -abs(y2 - y1);
    int sy = y1 < y2 ? 1 : -1;
    int error = dx + dy;
    for (;;) {
        int twice;
        surface_pixel(surface, clip, x1, y1, color_index);
        if (x1 == x2 && y1 == y2) break;
        twice = error * 2;
        if (twice >= dy) { error += dy; x1 += sx; }
        if (twice <= dx) { error += dx; y1 += sy; }
    }
}
