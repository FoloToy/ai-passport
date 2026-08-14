#pragma once

#include <stdbool.h>
#include <stdint.h>

#define TETRIS_BOARD_WIDTH  10
#define TETRIS_BOARD_HEIGHT 20

typedef enum {
    TETRIS_PIECE_NONE = 0,
    TETRIS_PIECE_I,
    TETRIS_PIECE_O,
    TETRIS_PIECE_T,
    TETRIS_PIECE_S,
    TETRIS_PIECE_Z,
    TETRIS_PIECE_J,
    TETRIS_PIECE_L,
} tetris_piece_t;

typedef enum {
    TETRIS_PLAYING = 0,
    TETRIS_PAUSED,
    TETRIS_GAME_OVER,
} tetris_state_t;

typedef enum {
    TETRIS_EVENT_NONE = 0,
    TETRIS_EVENT_MOVED,
    TETRIS_EVENT_LOCKED,
    TETRIS_EVENT_LINE_CLEAR,
    TETRIS_EVENT_GAME_OVER,
} tetris_event_t;

typedef struct {
    uint8_t board[TETRIS_BOARD_HEIGHT][TETRIS_BOARD_WIDTH];
    tetris_piece_t piece;
    tetris_piece_t next_piece;
    uint8_t rotation;
    int8_t x;
    int8_t y;
    uint32_t score;
    uint16_t lines;
    uint8_t level;
    tetris_state_t state;
    uint32_t rng;
    uint8_t bag[7];
    uint8_t bag_index;
} tetris_model_t;

void tetris_model_init(tetris_model_t *model, uint32_t seed);
void tetris_model_reset(tetris_model_t *model);
bool tetris_model_move(tetris_model_t *model, int dx);
bool tetris_model_rotate(tetris_model_t *model);
tetris_event_t tetris_model_step(tetris_model_t *model);
tetris_event_t tetris_model_hard_drop(tetris_model_t *model);
bool tetris_model_toggle_pause(tetris_model_t *model);
int tetris_model_ghost_y(const tetris_model_t *model);
uint32_t tetris_model_drop_interval_ms(const tetris_model_t *model);
bool tetris_piece_cell(tetris_piece_t piece, uint8_t rotation, int x, int y);
