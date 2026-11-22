// components/bsp/include/bsp_display_capture.h
// Screen-frame capture hook: while a capture is active, the BSP flush path
// hands every flushed area to the application callback (issue #98). Pure C
// header (no IDF/LVGL includes) so host tests compile it directly.
#pragma once
#include <stdbool.h>
#include <stdint.h>

// One flushed area of the screen frame. Coordinates are screen-space
// (origin top-left). pixels holds h rows of RGB565, little-endian (LVGL
// native - the panel link swaps bytes afterwards, which only matters when
// re-emitting bytes to the panel; black is 0 either way). stride is BYTES
// per row and is NOT w * 2: LVGL may pad rows, always step by stride.
// Areas arrive in flush order (unspecified for the application) and cover
// the whole frame only after a full refresh - accumulate them into your
// own buffer by coordinates. pixels is reused by the renderer: copy what
// you need inside the callback and never store the pointer.
// The callback runs inside the LVGL render task: do a bounded memcpy only.
// Never log per pixel, write UART/USB/flash, take the LVGL lock or block on
// a queue there - stream the copy from your own task after the frame.
typedef struct {
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    const uint8_t *pixels;
    uint32_t stride;
} bsp_display_capture_area_t;

typedef void (*bsp_display_capture_cb_t)(const bsp_display_capture_area_t *area);

// Start delivering flushed areas to cb. Returns false when cb is NULL or a
// capture is already active (no nesting: end() first). Call it where no
// flush can race it - under bsp_lvgl_lock(), or before bsp_lvgl_init().
// After begin, invalidate the screen (e.g. lv_obj_invalidate(
// lv_screen_active())); the next normal refresh produces the frame, area
// by area, with no additional full-screen buffer anywhere.
bool bsp_display_capture_begin(bsp_display_capture_cb_t cb);

// Stop delivering. Safe to call from inside the callback (delivery then
// stops after the current area) and when no capture is active.
void bsp_display_capture_end(void);

// Whether a capture is active (the flush path uses this to skip work).
bool bsp_display_capture_active(void);

// Flush-side feed: deliver one area when a capture is active. Device
// internal; exposed here so the host test can drive it.
void bsp_display_capture_record(int32_t x, int32_t y, int32_t w, int32_t h,
                                const uint8_t *pixels, uint32_t stride);
