#include <assert.h>
#include <stdio.h>

#include "game_puzzles.h"

int main(void)
{
    const uint8_t puzzle1_ok[] = { 7, 1, 3 };
    const uint8_t puzzle1_wrong_a[] = { 8, 1, 7 };
    const uint8_t puzzle1_wrong_b[] = { 1, 7, 3 };
    const uint8_t puzzle2_ok[] = { 1, 0, 0, 1 };
    const uint8_t puzzle2_wrong[] = { 4, 8, 2, 1 };

    assert(game_puzzle1_check(puzzle1_ok, 3));
    assert(!game_puzzle1_check(puzzle1_wrong_a, 3));
    assert(!game_puzzle1_check(puzzle1_wrong_b, 3));
    assert(!game_puzzle1_check(puzzle1_ok, 2));
    assert(game_puzzle2_check(puzzle2_ok, 4));
    assert(!game_puzzle2_check(puzzle2_wrong, 4));
    assert(!game_puzzle2_check(NULL, 4));

    assert(game_puzzle2_statement_is_true(0));
    assert(!game_puzzle2_statement_is_true(1));
    assert(!game_puzzle2_statement_is_true(2));
    assert(game_puzzle2_statement_is_true(3));
    assert(!game_puzzle2_statement_is_true(4));

    assert(game_ending_for_choice(GAME_FINAL_CHOICE_A) == GAME_ENDING_GUIDED);
    assert(game_ending_for_choice(GAME_FINAL_CHOICE_B) == GAME_ENDING_GAME_OVER);
    assert(game_ending_for_choice(GAME_FINAL_CHOICE_C) == GAME_ENDING_ESCAPED);
    assert(game_ending_for_choice(GAME_FINAL_CHOICE_D) == GAME_ENDING_TRUE_ESCAPE);
    assert(game_ending_for_choice(GAME_FINAL_CHOICE_NONE) == GAME_ENDING_NONE);

    puts("game_puzzles: all tests passed");
    return 0;
}
