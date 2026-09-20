#include <assert.h>
#include <stddef.h>

#include "bsp_display_rounding.h"

static void assert_span_matches_pixels(int width, int height, int radius)
{
    for (int y = 0; y < height; ++y) {
        int x1 = -1;
        int x2 = -1;
        assert(bsp_display_rounded_row_span(y, width, height, radius, &x1, &x2));
        for (int x = 0; x < width; ++x) {
            const bool expected_visible =
                !bsp_display_pixel_outside_rounded_rect(x, y, width, height, radius);
            const bool visible = x >= x1 && x <= x2;
            assert(visible == expected_visible);
        }
    }
}

int main(void)
{
    const int width = 240;
    const int height = 320;
    const int radius = 30;

    assert(bsp_display_pixel_outside_rounded_rect(0, 0, width, height, radius));
    assert(bsp_display_pixel_outside_rounded_rect(29, 0, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(30, 0, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(9, 9, width, height, radius));
    assert(bsp_display_pixel_outside_rounded_rect(8, 8, width, height, radius));

    assert(bsp_display_pixel_outside_rounded_rect(239, 0, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(209, 0, width, height, radius));
    assert(bsp_display_pixel_outside_rounded_rect(0, 319, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(30, 319, width, height, radius));
    assert(!bsp_display_pixel_outside_rounded_rect(120, 160, width, height, radius));

    assert(!bsp_display_pixel_outside_rounded_rect(0, 0, width, height, 0));
    assert(bsp_display_pixel_outside_rounded_rect(-1, 0, width, height, radius));

    // The optimized row-span path must be pixel-for-pixel equivalent at both
    // corners, the radius transition, and for clamped/degenerate radii.
    assert_span_matches_pixels(width, height, radius);
    assert_span_matches_pixels(17, 11, 4);
    assert_span_matches_pixels(17, 11, 99);
    assert_span_matches_pixels(17, 11, 0);

    int x1 = 123;
    int x2 = 456;
    assert(!bsp_display_rounded_row_span(-1, width, height, radius, &x1, &x2));
    assert(!bsp_display_rounded_row_span(0, 0, height, radius, &x1, &x2));
    assert(!bsp_display_rounded_row_span(0, width, height, radius, NULL, &x2));
    return 0;
}
