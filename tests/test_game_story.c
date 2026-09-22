#include <assert.h>
#include <stdio.h>

#include "game_story.h"

int main(void)
{
    assert(game_story_feedback_text(GAME_FEEDBACK_NONE) == GAME_TEXT_UNKNOWN);
    assert(game_story_feedback_text(GAME_FEEDBACK_PUZZLE_1_WRONG) ==
           GAME_TEXT_FEEDBACK_PUZZLE_1_WRONG);
    assert(game_story_feedback_text(GAME_FEEDBACK_PUZZLE_1_DIRECT_JOIN) ==
           GAME_TEXT_FEEDBACK_PUZZLE_1_DIRECT_JOIN);
    assert(game_story_feedback_text(GAME_FEEDBACK_PUZZLE_1_LOOK_AGAIN) ==
           GAME_TEXT_FEEDBACK_PUZZLE_1_LOOK_AGAIN);
    assert(game_story_feedback_text(GAME_FEEDBACK_PUZZLE_2_WRONG) ==
           GAME_TEXT_FEEDBACK_PUZZLE_2_WRONG);
    assert(game_story_feedback_text(GAME_FEEDBACK_PASSPORT_TRICK) ==
           GAME_TEXT_FEEDBACK_PASSPORT_TRICK);
    assert(game_story_feedback_text(GAME_FEEDBACK_CORRECT) ==
           GAME_TEXT_FEEDBACK_CORRECT);

    assert(game_story_hint_text(0) == GAME_TEXT_UNKNOWN);
    assert(game_story_hint_text(1) == GAME_TEXT_HINT_1);
    assert(game_story_hint_text(2) == GAME_TEXT_HINT_2);
    assert(game_story_hint_text(3) == GAME_TEXT_HINT_3);
    assert(game_story_hint_text(4) == GAME_TEXT_UNKNOWN);

    assert(game_story_ending_text(GAME_ENDING_GUIDED) ==
           GAME_TEXT_ENDING_STORY_GUIDED);
    assert(game_story_ending_text(GAME_ENDING_GAME_OVER) ==
           GAME_TEXT_ENDING_STORY_GAME_OVER);
    assert(game_story_ending_text(GAME_ENDING_ESCAPED) ==
           GAME_TEXT_ENDING_STORY_ESCAPED);
    assert(game_story_ending_text(GAME_ENDING_TRUE_ESCAPE) ==
           GAME_TEXT_ENDING_STORY_TRUE_ESCAPE);
    assert(!game_story_ending_congratulates(GAME_ENDING_GUIDED));
    assert(!game_story_ending_congratulates(GAME_ENDING_GAME_OVER));
    assert(game_story_ending_congratulates(GAME_ENDING_ESCAPED));
    assert(game_story_ending_congratulates(GAME_ENDING_TRUE_ESCAPE));

    puts("game_story: all tests passed");
    return 0;
}
