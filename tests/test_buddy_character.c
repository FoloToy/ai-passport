#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "buddy_character.h"

static void test_every_state_has_a_repeatable_nonempty_frame(void)
{
    const buddy_character_t states[] = {
        BUDDY_CHARACTER_SLEEP,
        BUDDY_CHARACTER_IDLE,
        BUDDY_CHARACTER_BUSY,
        BUDDY_CHARACTER_ATTENTION,
        BUDDY_CHARACTER_DIZZY,
        BUDDY_CHARACTER_HEART,
        BUDDY_CHARACTER_CELEBRATE,
        BUDDY_CHARACTER_PAIRING,
        BUDDY_CHARACTER_CONFIRMATION,
    };
    size_t index;

    for (index = 0; index < sizeof(states) / sizeof(states[0]); ++index) {
        const char *first = buddy_character_frame(states[index], 0);
        const char *wrapped = buddy_character_frame(states[index],
                                                    BUDDY_CHARACTER_ANIMATION_PERIOD_MS);

        assert(first != NULL);
        assert(first[0] != '\0');
        assert(wrapped != NULL);
        assert(strcmp(first, wrapped) == 0);
    }
}

static void test_attention_alternates_and_sleep_moves_slowly(void)
{
    const char *attention_first = buddy_character_frame(BUDDY_CHARACTER_ATTENTION, 0);
    const char *attention_second = buddy_character_frame(BUDDY_CHARACTER_ATTENTION, 500);
    const char *sleep_first = buddy_character_frame(BUDDY_CHARACTER_SLEEP, 0);
    const char *sleep_early = buddy_character_frame(BUDDY_CHARACTER_SLEEP, 500);
    const char *sleep_late = buddy_character_frame(BUDDY_CHARACTER_SLEEP, 1000);

    assert(attention_first != NULL);
    assert(attention_second != NULL);
    assert(strcmp(attention_first, attention_second) != 0);
    assert(sleep_first != NULL);
    assert(strcmp(sleep_first, sleep_early) == 0);
    assert(strcmp(sleep_first, sleep_late) != 0);
}

int main(void)
{
    test_every_state_has_a_repeatable_nonempty_frame();
    test_attention_alternates_and_sleep_moves_slowly();
    puts("buddy_character tests passed");
    return 0;
}
