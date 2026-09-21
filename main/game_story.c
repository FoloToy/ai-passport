#include "game_story.h"

game_text_id_t game_story_feedback_text(game_feedback_t feedback)
{
    switch (feedback) {
        case GAME_FEEDBACK_PUZZLE_1_WRONG:
            return GAME_TEXT_FEEDBACK_PUZZLE_1_WRONG;
        case GAME_FEEDBACK_PUZZLE_1_DIRECT_JOIN:
            return GAME_TEXT_FEEDBACK_PUZZLE_1_DIRECT_JOIN;
        case GAME_FEEDBACK_PUZZLE_1_LOOK_AGAIN:
            return GAME_TEXT_FEEDBACK_PUZZLE_1_LOOK_AGAIN;
        case GAME_FEEDBACK_PUZZLE_2_WRONG:
            return GAME_TEXT_FEEDBACK_PUZZLE_2_WRONG;
        case GAME_FEEDBACK_PASSPORT_TRICK:
            return GAME_TEXT_FEEDBACK_PASSPORT_TRICK;
        case GAME_FEEDBACK_CORRECT:
            return GAME_TEXT_FEEDBACK_CORRECT;
        case GAME_FEEDBACK_NONE:
        default:
            return GAME_TEXT_UNKNOWN;
    }
}

game_text_id_t game_story_hint_text(uint8_t hint_level)
{
    switch (hint_level) {
        case 1:
            return GAME_TEXT_HINT_1;
        case 2:
            return GAME_TEXT_HINT_2;
        case 3:
            return GAME_TEXT_HINT_3;
        default:
            return GAME_TEXT_UNKNOWN;
    }
}

game_text_id_t game_story_ending_text(game_ending_t ending)
{
    switch (ending) {
        case GAME_ENDING_GUIDED:
            return GAME_TEXT_ENDING_STORY_GUIDED;
        case GAME_ENDING_GAME_OVER:
            return GAME_TEXT_ENDING_STORY_GAME_OVER;
        case GAME_ENDING_ESCAPED:
            return GAME_TEXT_ENDING_STORY_ESCAPED;
        case GAME_ENDING_TRUE_ESCAPE:
            return GAME_TEXT_ENDING_STORY_TRUE_ESCAPE;
        case GAME_ENDING_NONE:
        default:
            return GAME_TEXT_UNKNOWN;
    }
}

bool game_story_ending_congratulates(game_ending_t ending)
{
    return ending == GAME_ENDING_ESCAPED ||
           ending == GAME_ENDING_TRUE_ESCAPE;
}
