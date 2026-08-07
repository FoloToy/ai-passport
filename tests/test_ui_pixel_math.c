#include <assert.h>
#include "ui_pixel_math.h"

int main(void)
{
    assert(ui_pixel_blink_frame(0) == 0);
    assert(ui_pixel_blink_frame(1700) == 1);
    assert(ui_pixel_blink_frame(1850) == 0);

    assert(ui_pixel_jump_offset(0) == 0);
    assert(ui_pixel_jump_offset(1) == -3);
    assert(ui_pixel_jump_offset(2) == -5);
    assert(ui_pixel_jump_offset(3) == -3);
    assert(ui_pixel_jump_offset(4) == 0);
    assert(ui_pixel_jump_offset(99) == 0);
    return 0;
}
