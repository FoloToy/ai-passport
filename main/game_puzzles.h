#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    GAME_FINAL_CHOICE_NONE = 0,
    GAME_FINAL_CHOICE_A,
    GAME_FINAL_CHOICE_B,
    GAME_FINAL_CHOICE_C,
    GAME_FINAL_CHOICE_D,
} game_final_choice_t;

typedef enum {
    GAME_ENDING_NONE = 0,
    GAME_ENDING_GUIDED,
    GAME_ENDING_GAME_OVER,
    GAME_ENDING_ESCAPED,
    GAME_ENDING_TRUE_ESCAPE,
} game_ending_t;

bool game_puzzle1_check(const uint8_t *digits, size_t count);
bool game_puzzle2_check(const uint8_t *digits, size_t count);
bool game_puzzle2_statement_is_true(size_t statement_index);
game_ending_t game_ending_for_choice(game_final_choice_t choice);
