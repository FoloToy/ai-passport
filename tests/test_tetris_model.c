#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "tetris_model.h"

static int occupied(const tetris_model_t *model) {
    int count = 0;
    for (int y = 0; y < TETRIS_BOARD_HEIGHT; y++) {
        for (int x = 0; x < TETRIS_BOARD_WIDTH; x++) count += model->board[y][x] != 0;
    }
    return count;
}

static void test_initial_state(void) {
    tetris_model_t model;
    tetris_model_init(&model, 1234);
    assert(model.state == TETRIS_PLAYING);
    assert(model.piece >= TETRIS_PIECE_I && model.piece <= TETRIS_PIECE_L);
    assert(model.next_piece >= TETRIS_PIECE_I && model.next_piece <= TETRIS_PIECE_L);
    assert(model.level == 1 && model.score == 0 && model.lines == 0);
    assert(occupied(&model) == 0);
}

static void test_wall_collision_and_rotation(void) {
    tetris_model_t model;
    tetris_model_init(&model, 42);
    model.piece = TETRIS_PIECE_T;
    model.rotation = 0;
    model.x = 0;
    assert(tetris_model_move(&model, -1) == false);
    assert(tetris_model_rotate(&model));
}

static void test_hard_drop_locks_piece(void) {
    tetris_model_t model;
    tetris_model_init(&model, 99);
    tetris_event_t event = tetris_model_hard_drop(&model);
    assert(event == TETRIS_EVENT_LOCKED);
    assert(occupied(&model) == 4);
    assert(model.score > 0);
}

static void test_line_clear_and_score(void) {
    tetris_model_t model;
    tetris_model_init(&model, 7);
    memset(model.board, 0, sizeof(model.board));
    for (int x = 0; x < 6; x++) model.board[19][x] = TETRIS_PIECE_J;
    model.piece = TETRIS_PIECE_I;
    model.rotation = 0;
    model.x = 6;
    model.y = 18;
    tetris_event_t event = tetris_model_hard_drop(&model);
    assert(event == TETRIS_EVENT_LINE_CLEAR);
    assert(model.lines == 1);
    assert(model.score == 100);
    assert(occupied(&model) == 0);
}

static void test_pause_and_speed(void) {
    tetris_model_t model;
    tetris_model_init(&model, 88);
    assert(tetris_model_toggle_pause(&model));
    assert(model.state == TETRIS_PAUSED);
    assert(tetris_model_step(&model) == TETRIS_EVENT_NONE);
    assert(tetris_model_toggle_pause(&model));
    model.level = 20;
    assert(tetris_model_drop_interval_ms(&model) == 150);
}

int main(void) {
    test_initial_state();
    test_wall_collision_and_rotation();
    test_hard_drop_locks_piece();
    test_line_clear_and_score();
    test_pause_and_speed();
    puts("tetris_model: all tests passed");
    return 0;
}
