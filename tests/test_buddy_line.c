#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "buddy_line.h"

typedef struct {
    char lines[4][BUDDY_JSON_LINE_MAX + 1];
    size_t lengths[4];
    size_t count;
} line_capture_t;

static void capture_line(const char *line, size_t length, void *context)
{
    line_capture_t *capture = context;

    assert(capture->count < sizeof(capture->lines) / sizeof(capture->lines[0]));
    assert(length <= BUDDY_JSON_LINE_MAX);
    memcpy(capture->lines[capture->count], line, length);
    capture->lines[capture->count][length] = '\0';
    capture->lengths[capture->count] = length;
    ++capture->count;
}

static void test_split_and_multiple_lines(void)
{
    buddy_line_buffer_t rx;
    line_capture_t capture = {0};

    buddy_line_init(&rx);
    assert(buddy_line_push(&rx, (const uint8_t *)"{\"a\":", 5, capture_line, &capture) ==
           BUDDY_LINE_OK);
    assert(buddy_line_push(&rx, (const uint8_t *)"1}\n{\"b\":2}\n", 13, capture_line, &capture) ==
           BUDDY_LINE_OK);
    assert(capture.count == 2);
    assert(strcmp(capture.lines[0], "{\"a\":1}") == 0);
    assert(strcmp(capture.lines[1], "{\"b\":2}") == 0);
}

static void test_crlf_is_trimmed(void)
{
    buddy_line_buffer_t rx;
    line_capture_t capture = {0};

    buddy_line_init(&rx);
    assert(buddy_line_push(&rx, (const uint8_t *)"one\r\ntwo\n", 9, capture_line, &capture) ==
           BUDDY_LINE_OK);
    assert(capture.count == 2);
    assert(strcmp(capture.lines[0], "one") == 0);
    assert(strcmp(capture.lines[1], "two") == 0);
}

static void test_exact_limit_line_is_delivered(void)
{
    buddy_line_buffer_t rx;
    line_capture_t capture = {0};
    char input[BUDDY_JSON_LINE_MAX + 1];

    memset(input, 'x', BUDDY_JSON_LINE_MAX);
    input[BUDDY_JSON_LINE_MAX] = '\n';
    buddy_line_init(&rx);
    assert(buddy_line_push(&rx, (const uint8_t *)input, sizeof(input), capture_line, &capture) ==
           BUDDY_LINE_OK);
    assert(capture.count == 1);
    assert(capture.lengths[0] == BUDDY_JSON_LINE_MAX);
    assert(capture.lines[0][BUDDY_JSON_LINE_MAX] == '\0');
}

static void test_overflow_discards_through_newline_then_recovers(void)
{
    buddy_line_buffer_t rx;
    line_capture_t capture = {0};
    char oversized[BUDDY_JSON_LINE_MAX + 2];

    memset(oversized, 'x', BUDDY_JSON_LINE_MAX + 1);
    oversized[BUDDY_JSON_LINE_MAX + 1] = '\n';
    buddy_line_init(&rx);
    assert(buddy_line_push(&rx, (const uint8_t *)oversized, sizeof(oversized), capture_line,
                           &capture) == BUDDY_LINE_OVERFLOW);
    assert(capture.count == 0);
    assert(buddy_line_push(&rx, (const uint8_t *)"{\"ok\":true}\n", 12, capture_line, &capture) ==
           BUDDY_LINE_OK);
    assert(capture.count == 1);
    assert(strcmp(capture.lines[0], "{\"ok\":true}") == 0);
}

int main(void)
{
    test_split_and_multiple_lines();
    test_crlf_is_trimmed();
    test_exact_limit_line_is_delivered();
    test_overflow_discards_through_newline_then_recovers();
    return 0;
}
