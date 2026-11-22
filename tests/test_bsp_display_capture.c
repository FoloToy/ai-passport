#include <assert.h>
#include <stddef.h>

#include "bsp_display_capture.h"

static bsp_display_capture_area_t s_last;
static int s_calls;

static void sink(const bsp_display_capture_area_t *area)
{
    s_last = *area;
    s_calls++;
}

static void ender(const bsp_display_capture_area_t *area)
{
    (void)area;
    bsp_display_capture_end();
}

int main(void)
{
    static uint8_t buf[64];

    // Inactive: record delivers nothing.
    bsp_display_capture_record(0, 0, 8, 4, buf, 16);
    assert(s_calls == 0);
    assert(!bsp_display_capture_active());

    // begin validates the callback and refuses nesting.
    assert(!bsp_display_capture_begin(NULL));
    assert(bsp_display_capture_begin(sink));
    assert(!bsp_display_capture_begin(sink));
    assert(bsp_display_capture_active());

    // record maps every field verbatim; stride passes through unpadded.
    bsp_display_capture_record(12, 34, 56, 78, buf, 120);
    assert(s_calls == 1);
    assert(s_last.x == 12 && s_last.y == 34);
    assert(s_last.w == 56 && s_last.h == 78);
    assert(s_last.pixels == buf && s_last.stride == 120);

    // Empty or NULL areas are dropped silently.
    bsp_display_capture_record(0, 0, 0, 4, buf, 16);
    bsp_display_capture_record(0, 0, 4, 0, buf, 16);
    bsp_display_capture_record(0, 0, 4, 4, NULL, 16);
    assert(s_calls == 1);
    bsp_display_capture_end();

    // end() from inside the callback stops delivery after the current area.
    assert(bsp_display_capture_begin(ender));
    bsp_display_capture_record(1, 2, 3, 4, buf, 6);
    assert(!bsp_display_capture_active());
    bsp_display_capture_record(1, 2, 3, 4, buf, 6);
    assert(s_calls == 1);

    // end() is safe when idle; a fresh begin works afterwards.
    bsp_display_capture_end();
    assert(bsp_display_capture_begin(sink));
    bsp_display_capture_record(0, 0, 1, 1, buf, 2);
    assert(s_calls == 2);
    bsp_display_capture_end();

    return 0;
}
