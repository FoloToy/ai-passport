#include "bsp_display_rounding.h"

static int32_t clamp_radius(int32_t width, int32_t height, int32_t radius)
{
    if (radius <= 0 || width <= 0 || height <= 0) return 0;
    const int32_t max_radius = (width < height ? width : height) / 2;
    return radius > max_radius ? max_radius : radius;
}

bool bsp_display_rounded_row_span(int32_t y, int32_t width, int32_t height,
                                  int32_t radius, int32_t *x1, int32_t *x2)
{
    if (!x1 || !x2 || width <= 0 || height <= 0 || y < 0 || y >= height) {
        return false;
    }
    radius = clamp_radius(width, height, radius);
    if (radius <= 0 || (y >= radius && y < height - radius)) {
        *x1 = 0;
        *x2 = width - 1;
        return true;
    }

    const int32_t edge_y = y < radius ? radius - y : y - (height - 1 - radius);
    int32_t inset = 0;
    while ((inset + 1) * (inset + 1) + edge_y * edge_y <= radius * radius) {
        ++inset;
    }
    // The loop above is at most 30 iterations for this board and runs only
    // once per edge row, instead of once per pixel in every flush.
    *x1 = radius - inset;
    *x2 = width - radius + inset - 1;
    if (*x1 < 0) *x1 = 0;
    if (*x2 >= width) *x2 = width - 1;
    return *x1 <= *x2;
}

bool bsp_display_pixel_outside_rounded_rect(int32_t x, int32_t y,
                                            int32_t width, int32_t height,
                                            int32_t radius)
{
    if (x < 0 || y < 0 || x >= width || y >= height) return true;
    if (radius <= 0 || width <= 0 || height <= 0) return false;

    radius = clamp_radius(width, height, radius);

    int32_t dx;
    if (x < radius) {
        dx = radius - x;
    } else if (x >= width - radius) {
        dx = x - (width - 1 - radius);
    } else {
        return false;
    }

    int32_t dy;
    if (y < radius) {
        dy = radius - y;
    } else if (y >= height - radius) {
        dy = y - (height - 1 - radius);
    } else {
        return false;
    }

    return dx * dx + dy * dy > radius * radius;
}
