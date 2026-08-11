#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "buddy_types.h"

typedef enum {
    BUDDY_LINE_OK,
    BUDDY_LINE_OVERFLOW,
} buddy_line_result_t;

typedef void (*buddy_line_callback_t)(const char *line, size_t length, void *context);

typedef struct {
    char data[BUDDY_JSON_LINE_MAX + 1];
    size_t length;
    bool discarding;
} buddy_line_buffer_t;

void buddy_line_init(buddy_line_buffer_t *buffer);
buddy_line_result_t buddy_line_push(buddy_line_buffer_t *buffer, const uint8_t *bytes,
                                    size_t length, buddy_line_callback_t callback, void *context);
