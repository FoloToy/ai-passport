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

typedef struct {
    line_capture_t capture;
    bool gate_open;
} gated_line_capture_t;

static bool capture_line(const char *line, size_t length, void *context)
{
    line_capture_t *capture = context;

    assert(capture->count < sizeof(capture->lines) / sizeof(capture->lines[0]));
    assert(length <= BUDDY_JSON_LINE_MAX);
    memcpy(capture->lines[capture->count], line, length);
    capture->lines[capture->count][length] = '\0';
    capture->lengths[capture->count] = length;
    ++capture->count;
    return true;
}

static bool capture_line_then_revoke_gate(const char *line, size_t length, void *context)
{
    gated_line_capture_t *gated = context;

    assert(gated->gate_open);
    (void)capture_line(line, length, &gated->capture);
    gated->gate_open = false;
    return gated->gate_open;
}

static void test_split_and_multiple_lines(void)
{
    buddy_line_buffer_t rx;
    line_capture_t capture = {0};
    static const char third_write[] = "1}\n{\"b\":2}\n";

    buddy_line_init(&rx);
    assert(buddy_line_push(&rx, (const uint8_t *)"{\"", 2, capture_line, &capture) ==
           BUDDY_LINE_OK);
    assert(buddy_line_push(&rx, (const uint8_t *)"a\":", 3, capture_line, &capture) ==
           BUDDY_LINE_OK);
    assert(buddy_line_push(&rx, (const uint8_t *)third_write, sizeof(third_write) - 1,
                           capture_line, &capture) ==
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

static void test_callback_can_abort_remaining_lines_and_reset_buffer(void)
{
    buddy_line_buffer_t rx;
    gated_line_capture_t gated = {
        .gate_open = true,
    };

    buddy_line_init(&rx);
    assert(buddy_line_push(&rx, (const uint8_t *)"one\ntwo\npartial", 15,
                           capture_line_then_revoke_gate, &gated) == BUDDY_LINE_ABORTED);
    assert(!gated.gate_open);
    assert(gated.capture.count == 1);
    assert(strcmp(gated.capture.lines[0], "one") == 0);
    assert(rx.length == 0);
    assert(!rx.discarding);
}

int main(void)
{
    test_split_and_multiple_lines();
    test_crlf_is_trimmed();
    test_exact_limit_line_is_delivered();
    test_overflow_discards_through_newline_then_recovers();
    test_callback_can_abort_remaining_lines_and_reset_buffer();
    return 0;
}
