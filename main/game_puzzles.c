#include "game_puzzles.h"

bool game_puzzle1_check(const uint8_t *digits, size_t count)
{
    return digits && count == 3 &&
           digits[0] == 7 && digits[1] == 1 && digits[2] == 3;
}

bool game_puzzle2_check(const uint8_t *digits, size_t count)
{
    return digits && count == 4 &&
           digits[0] == 1 && digits[1] == 0 &&
           digits[2] == 0 && digits[3] == 1;
}

bool game_puzzle2_statement_is_true(size_t statement_index)
{
    static const bool truth[] = { true, false, false, true };
    return statement_index < sizeof(truth) / sizeof(truth[0]) && truth[statement_index];
}

game_ending_t game_ending_for_choice(game_final_choice_t choice)
{
    switch (choice) {
        case GAME_FINAL_CHOICE_A:
            return GAME_ENDING_GUIDED;
        case GAME_FINAL_CHOICE_B:
            return GAME_ENDING_GAME_OVER;
        case GAME_FINAL_CHOICE_C:
            return GAME_ENDING_ESCAPED;
        case GAME_FINAL_CHOICE_D:
            return GAME_ENDING_TRUE_ESCAPE;
        case GAME_FINAL_CHOICE_NONE:
        default:
            return GAME_ENDING_NONE;
    }
}
