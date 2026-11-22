// components/bsp/src/bsp_display_capture.c - capture state machine (pure).
#include "bsp_display_capture.h"
#include <stddef.h>

static bsp_display_capture_cb_t s_cb;

bool bsp_display_capture_begin(bsp_display_capture_cb_t cb)
{
    if (!cb || s_cb) return false;
    s_cb = cb;
    return true;
}

void bsp_display_capture_end(void)
{
    s_cb = NULL;
}

bool bsp_display_capture_active(void)
{
    return s_cb != NULL;
}

void bsp_display_capture_record(int32_t x, int32_t y, int32_t w, int32_t h,
                                const uint8_t *pixels, uint32_t stride)
{
    bsp_display_capture_cb_t cb = s_cb;   // end() inside cb stops the NEXT area
    if (!cb || !pixels || w <= 0 || h <= 0) return;
    const bsp_display_capture_area_t area = {
        .x = x, .y = y, .w = w, .h = h,
        .pixels = pixels, .stride = stride,
    };
    cb(&area);
}
