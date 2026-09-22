#include <assert.h>
#include <stdio.h>

#include "game_timer.h"

int main(void)
{
    game_timer_t timer;
    game_timer_init(&timer, GAME_DURATION_SECONDS);
    assert(game_timer_remaining(&timer, 1234) == 900);

    game_timer_start(&timer, 1000);
    assert(game_timer_remaining(&timer, 1000) == 900);
    assert(game_timer_remaining(&timer, 2000) == 899);
    assert(game_timer_poll(&timer, 600999) == GAME_TIMER_EVENT_NONE);
    assert(game_timer_poll(&timer, 601000) == GAME_TIMER_EVENT_FIVE_MINUTES);
    assert(game_timer_poll(&timer, 601000) == GAME_TIMER_EVENT_NONE);
    assert(game_timer_poll(&timer, 721000) == GAME_TIMER_EVENT_THREE_MINUTES);
    assert(game_timer_poll(&timer, 841000) == GAME_TIMER_EVENT_ONE_MINUTE);
    assert(game_timer_poll(&timer, 901000) == GAME_TIMER_EVENT_TIMEOUT);
    assert(game_timer_remaining(&timer, 901000) == 0);
    assert(game_timer_poll(&timer, 902000) == GAME_TIMER_EVENT_NONE);

    puts("game_timer: all tests passed");
    return 0;
}
