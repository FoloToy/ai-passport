#pragma once

#include <stdbool.h>
#include <stdint.h>

#define GAME_DURATION_SECONDS (15U * 60U)

typedef enum {
    GAME_TIMER_EVENT_NONE = 0,
    GAME_TIMER_EVENT_FIVE_MINUTES = 1U << 0,
    GAME_TIMER_EVENT_THREE_MINUTES = 1U << 1,
    GAME_TIMER_EVENT_ONE_MINUTE = 1U << 2,
    GAME_TIMER_EVENT_TIMEOUT = 1U << 3,
} game_timer_event_t;

typedef struct {
    uint32_t duration_seconds;
    uint64_t deadline_ms;
    uint8_t emitted_events;
    bool started;
    bool expired;
} game_timer_t;

void game_timer_init(game_timer_t *timer, uint32_t duration_seconds);
void game_timer_start(game_timer_t *timer, uint64_t now_ms);
uint32_t game_timer_remaining(const game_timer_t *timer, uint64_t now_ms);
uint8_t game_timer_poll(game_timer_t *timer, uint64_t now_ms);
