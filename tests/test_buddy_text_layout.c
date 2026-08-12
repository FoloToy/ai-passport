#include <assert.h>
#include <string.h>

#include "buddy_text_layout.h"

static unsigned width(const char *text, size_t length, void *context)
{
    (void)text; (void)context;
    return (unsigned)length;
}

int main(void)
{
    char output[64];
    buddy_text_result_t result = buddy_text_wrap("ONE TWO THREE FOUR", output,
                                                 sizeof(output), 7, 2, width, NULL);
    assert(strcmp(output, "ONE TWO\nTHRE...") == 0);
    assert(result.lines == 2 && result.truncated);
    result = buddy_text_wrap("SUPERCALIFRAGILISTIC", output, sizeof(output), 8, 1, width, NULL);
    assert(strcmp(output, "SUPER...") == 0 && result.truncated);
    assert(BUDDY_UI_STATUS_Y == 0 && BUDDY_UI_STATUS_H == 26);
    assert(BUDDY_UI_STAGE_Y == 26 && BUDDY_UI_STAGE_H == 132);
    assert(BUDDY_UI_INFO_Y == 158 && BUDDY_UI_INFO_H == 138);
    assert(BUDDY_UI_ACTION_Y == 296 && BUDDY_UI_ACTION_H == 24);
    assert(strlen(BUDDY_ACTION_HOME) * 8U <= 224U);
    assert(strlen(BUDDY_ACTION_PET) * 8U <= 224U);
    assert(strlen(BUDDY_ACTION_INFO) * 8U <= 224U);
    assert(strlen(BUDDY_ACTION_SETTINGS) * 8U <= 224U);
    assert(strlen(BUDDY_ACTION_CONFIRM) * 8U <= 204U);
    assert(strlen(BUDDY_ACTION_APPROVAL) * 8U <= 204U);
    assert(buddy_overlay_select(true, true, true, true) == BUDDY_OVERLAY_CONFIRMATION);
    assert(buddy_overlay_select(false, true, true, true) == BUDDY_OVERLAY_PAIRING);
    assert(buddy_overlay_select(false, false, true, true) == BUDDY_OVERLAY_APPROVAL);
    assert(buddy_overlay_select(false, false, false, true) == BUDDY_OVERLAY_MENU);
    return 0;
}
