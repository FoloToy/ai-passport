#include "buddy_line.h"

#include <string.h>

void buddy_line_init(buddy_line_buffer_t *buffer)
{
    if (buffer != NULL) {
        memset(buffer, 0, sizeof(*buffer));
    }
}

buddy_line_result_t buddy_line_push(buddy_line_buffer_t *buffer, const uint8_t *bytes,
                                    size_t length, buddy_line_callback_t callback, void *context)
{
    buddy_line_result_t result = BUDDY_LINE_OK;
    size_t index;

    if (buffer == NULL || bytes == NULL) {
        return result;
    }

    for (index = 0; index < length; ++index) {
        char byte = (char)bytes[index];

        if (buffer->discarding) {
            if (byte == '\n') {
                buffer->discarding = false;
                result = BUDDY_LINE_OVERFLOW;
            }
            continue;
        }
        if (byte == '\n') {
            size_t line_length = buffer->length;

            if (line_length > 0 && buffer->data[line_length - 1] == '\r') {
                --line_length;
            }
            buffer->data[line_length] = '\0';
            if (callback != NULL) {
                callback(buffer->data, line_length, context);
            }
            buffer->length = 0;
            continue;
        }
        if (buffer->length == BUDDY_JSON_LINE_MAX) {
            buffer->length = 0;
            buffer->discarding = true;
            continue;
        }
        buffer->data[buffer->length++] = byte;
    }
    return result;
}
