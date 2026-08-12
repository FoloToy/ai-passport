#pragma once

#include <stdbool.h>
#include <stddef.h>

#define BUDDY_UI_STATUS_Y 0
#define BUDDY_UI_STATUS_H 26
#define BUDDY_UI_STAGE_Y 26
#define BUDDY_UI_STAGE_H 132
#define BUDDY_UI_INFO_Y 158
#define BUDDY_UI_INFO_H 138
#define BUDDY_UI_ACTION_Y 296
#define BUDDY_UI_ACTION_H 24

typedef unsigned (*buddy_text_measure_fn)(const char *text, size_t length, void *context);

typedef struct {
    unsigned lines;
    bool truncated;
} buddy_text_result_t;

typedef enum {
    BUDDY_OVERLAY_NONE,
    BUDDY_OVERLAY_MENU,
    BUDDY_OVERLAY_APPROVAL,
    BUDDY_OVERLAY_PAIRING,
    BUDDY_OVERLAY_CONFIRMATION,
} buddy_overlay_kind_t;

buddy_text_result_t buddy_text_wrap(const char *input, char *output, size_t output_size,
                                    unsigned max_width, unsigned max_lines,
                                    buddy_text_measure_fn measure, void *context);
buddy_overlay_kind_t buddy_overlay_select(bool confirmation, bool pairing,
                                          bool approval, bool menu);
