#include "ui_pixel_math.h"

int ui_pixel_blink_frame(uint32_t elapsed_ms)
{
    uint32_t phase = elapsed_ms % 2000U;
    return phase >= 1650U && phase < 1800U;
}

int ui_pixel_jump_offset(unsigned frame)
{
    static const int offsets[] = { 0, -3, -5, -3, 0 };
    return frame < sizeof(offsets) / sizeof(offsets[0]) ? offsets[frame] : 0;
}
