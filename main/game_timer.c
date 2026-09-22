#include "game_timer.h"

#include <stddef.h>

void game_timer_init(game_timer_t *timer, uint32_t duration_seconds)
{
    if (!timer) return;
    timer->duration_seconds = duration_seconds;
    timer->deadline_ms = 0;
    timer->emitted_events = 0;
    timer->started = false;
    timer->expired = false;
}

void game_timer_start(game_timer_t *timer, uint64_t now_ms)
{
    if (!timer) return;
    timer->deadline_ms = now_ms + (uint64_t)timer->duration_seconds * 1000ULL;
    timer->emitted_events = 0;
    timer->started = true;
    timer->expired = false;
}

uint32_t game_timer_remaining(const game_timer_t *timer, uint64_t now_ms)
{
    if (!timer) return 0;
    if (!timer->started) return timer->duration_seconds;
    if (timer->expired || now_ms >= timer->deadline_ms) return 0;

    uint64_t remaining_ms = timer->deadline_ms - now_ms;
    return (uint32_t)((remaining_ms + 999ULL) / 1000ULL);
}

uint8_t game_timer_poll(game_timer_t *timer, uint64_t now_ms)
{
    if (!timer || !timer->started || timer->expired) return GAME_TIMER_EVENT_NONE;

    uint8_t events = GAME_TIMER_EVENT_NONE;
    uint32_t remaining = game_timer_remaining(timer, now_ms);
    const uint8_t warnings[] = {
        GAME_TIMER_EVENT_FIVE_MINUTES,
        GAME_TIMER_EVENT_THREE_MINUTES,
        GAME_TIMER_EVENT_ONE_MINUTE,
    };
    const uint32_t thresholds[] = { 5U * 60U, 3U * 60U, 60U };

    for (unsigned i = 0; i < sizeof(warnings) / sizeof(warnings[0]); ++i) {
        if (remaining <= thresholds[i] && !(timer->emitted_events & warnings[i])) {
            timer->emitted_events |= warnings[i];
            events |= warnings[i];
        }
    }
    if (remaining == 0) {
        timer->expired = true;
        timer->emitted_events |= GAME_TIMER_EVENT_TIMEOUT;
        events |= GAME_TIMER_EVENT_TIMEOUT;
    }
    return events;
}
