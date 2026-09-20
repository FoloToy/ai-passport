#pragma once

#include <stdbool.h>
#include <stdint.h>

// Pure geometry helper kept independent from ESP-IDF/LVGL for host tests.
bool bsp_display_pixel_outside_rounded_rect(int32_t x, int32_t y,
                                            int32_t width, int32_t height,
                                            int32_t radius);

// Return the inclusive horizontal span that remains visible on an edge row.
// The span is empty when the row is outside the rounded rectangle.
bool bsp_display_rounded_row_span(int32_t y, int32_t width, int32_t height,
                                  int32_t radius, int32_t *x1, int32_t *x2);
