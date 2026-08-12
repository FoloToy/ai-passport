#include "buddy_text_layout.h"

#include <string.h>

static void append(char *output, size_t size, size_t *used, const char *text, size_t length)
{
    size_t room = size > *used ? size - *used - 1U : 0;
    if (length > room) length = room;
    if (length) memcpy(output + *used, text, length);
    *used += length;
    if (size) output[*used < size ? *used : size - 1U] = '\0';
}

buddy_text_result_t buddy_text_wrap(const char *input, char *output, size_t output_size,
                                    unsigned max_width, unsigned max_lines,
                                    buddy_text_measure_fn measure, void *context)
{
    buddy_text_result_t result = {0};
    const char *cursor = input != NULL ? input : "";
    size_t used = 0;
    if (output_size) output[0] = '\0';
    if (!measure || max_width == 0 || max_lines == 0) return result;
    while (*cursor && result.lines < max_lines) {
        const char *line = cursor;
        const char *last_space = NULL;
        const char *end = cursor;
        while (*end && *end != '\n' && measure(line, (size_t)(end - line + 1), context) <= max_width) {
            if (*end == ' ') last_space = end;
            ++end;
        }
        if (*end == ' ') last_space = end;
        if (*end == '\n') {
            append(output, output_size, &used, line, (size_t)(end - line));
            cursor = end + 1;
        } else if (*end == '\0') {
            append(output, output_size, &used, line, (size_t)(end - line));
            cursor = end;
        } else if (last_space != NULL) {
            append(output, output_size, &used, line, (size_t)(last_space - line));
            cursor = last_space + 1;
        } else {
            size_t fit = (size_t)(end - line);
            while (fit > 3 && measure(line, fit - 3 + 3, context) > max_width) --fit;
            if (fit > 3) fit -= 3;
            append(output, output_size, &used, line, fit);
            append(output, output_size, &used, "...", 3);
            while (*end && *end != ' ' && *end != '\n') ++end;
            cursor = *end ? end + 1 : end;
            result.truncated = true;
        }
        ++result.lines;
        if (*cursor && result.lines < max_lines) append(output, output_size, &used, "\n", 1);
    }
    if (*cursor) {
        char *last_line = strrchr(output, '\n');
        size_t prefix;
        size_t length;
        last_line = last_line != NULL ? last_line + 1 : output;
        prefix = (size_t)(last_line - output);
        length = strlen(last_line);
        while (length && last_line[length - 1] == ' ') last_line[--length] = '\0';
        while (length && measure(last_line, length, context) + measure("...", 3, context) > max_width)
            --length;
        used = prefix + length;
        output[used] = '\0';
        append(output, output_size, &used, "...", 3);
        result.truncated = true;
    }
    return result;
}

buddy_overlay_kind_t buddy_overlay_select(bool confirmation, bool pairing,
                                          bool approval, bool menu)
{
    if (confirmation) return BUDDY_OVERLAY_CONFIRMATION;
    if (pairing) return BUDDY_OVERLAY_PAIRING;
    if (approval) return BUDDY_OVERLAY_APPROVAL;
    if (menu) return BUDDY_OVERLAY_MENU;
    return BUDDY_OVERLAY_NONE;
}
