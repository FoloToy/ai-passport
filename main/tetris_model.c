#include "tetris_model.h"

#include <string.h>

static const uint16_t SHAPES[7][4] = {
    { 0x00F0, 0x4444, 0x0F00, 0x2222 }, /* I */
    { 0x0066, 0x0066, 0x0066, 0x0066 }, /* O */
    { 0x0072, 0x0262, 0x0270, 0x0232 }, /* T */
    { 0x0036, 0x0462, 0x0360, 0x0231 }, /* S */
    { 0x0063, 0x0264, 0x0630, 0x0132 }, /* Z */
    { 0x0071, 0x0226, 0x0470, 0x0322 }, /* J */
    { 0x0074, 0x0622, 0x0170, 0x0223 }, /* L */
};

bool tetris_piece_cell(tetris_piece_t piece, uint8_t rotation, int x, int y) {
    if (piece < TETRIS_PIECE_I || piece > TETRIS_PIECE_L ||
        x < 0 || x >= 4 || y < 0 || y >= 4) {
        return false;
    }
    uint16_t mask = SHAPES[piece - 1][rotation & 3U];
    return (mask & (1U << (y * 4 + x))) != 0;
}

static uint32_t random_next(tetris_model_t *model) {
    uint32_t x = model->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    model->rng = x ? x : 0x6D2B79F5U;
    return model->rng;
}

static tetris_piece_t bag_take(tetris_model_t *model) {
    if (model->bag_index >= 7) {
        for (uint8_t i = 0; i < 7; i++) model->bag[i] = i + 1;
        for (int i = 6; i > 0; i--) {
            int j = (int)(random_next(model) % (uint32_t)(i + 1));
            uint8_t tmp = model->bag[i];
            model->bag[i] = model->bag[j];
            model->bag[j] = tmp;
        }
        model->bag_index = 0;
    }
    return (tetris_piece_t)model->bag[model->bag_index++];
}

static bool collides(const tetris_model_t *model, tetris_piece_t piece,
                     uint8_t rotation, int x, int y) {
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!tetris_piece_cell(piece, rotation, px, py)) continue;
            int bx = x + px;
            int by = y + py;
            if (bx < 0 || bx >= TETRIS_BOARD_WIDTH || by >= TETRIS_BOARD_HEIGHT) return true;
            if (by >= 0 && model->board[by][bx] != 0) return true;
        }
    }
    return false;
}

static bool spawn_piece(tetris_model_t *model) {
    model->piece = model->next_piece;
    model->next_piece = bag_take(model);
    model->rotation = 0;
    model->x = 3;
    model->y = 0;
    if (collides(model, model->piece, model->rotation, model->x, model->y)) {
        model->state = TETRIS_GAME_OVER;
        return false;
    }
    return true;
}

void tetris_model_reset(tetris_model_t *model) {
    uint32_t seed = model->rng ? model->rng : 0xC001D00DU;
    memset(model, 0, sizeof(*model));
    model->rng = seed;
    model->bag_index = 7;
    model->level = 1;
    model->state = TETRIS_PLAYING;
    model->next_piece = bag_take(model);
    spawn_piece(model);
}

void tetris_model_init(tetris_model_t *model, uint32_t seed) {
    memset(model, 0, sizeof(*model));
    model->rng = seed ? seed : 0xC001D00DU;
    tetris_model_reset(model);
}

bool tetris_model_move(tetris_model_t *model, int dx) {
    if (model->state != TETRIS_PLAYING || (dx != -1 && dx != 1)) return false;
    if (collides(model, model->piece, model->rotation, model->x + dx, model->y)) return false;
    model->x += dx;
    return true;
}

bool tetris_model_rotate(tetris_model_t *model) {
    static const int8_t KICKS[] = { 0, -1, 1, -2, 2 };
    if (model->state != TETRIS_PLAYING) return false;
    uint8_t next_rotation = (model->rotation + 1U) & 3U;
    for (size_t i = 0; i < sizeof(KICKS) / sizeof(KICKS[0]); i++) {
        int next_x = model->x + KICKS[i];
        if (!collides(model, model->piece, next_rotation, next_x, model->y)) {
            model->rotation = next_rotation;
            model->x = next_x;
            return true;
        }
    }
    return false;
}

static int clear_lines(tetris_model_t *model) {
    int cleared = 0;
    for (int y = TETRIS_BOARD_HEIGHT - 1; y >= 0; y--) {
        bool full = true;
        for (int x = 0; x < TETRIS_BOARD_WIDTH; x++) {
            if (model->board[y][x] == 0) {
                full = false;
                break;
            }
        }
        if (!full) continue;
        for (int copy_y = y; copy_y > 0; copy_y--) {
            memcpy(model->board[copy_y], model->board[copy_y - 1], TETRIS_BOARD_WIDTH);
        }
        memset(model->board[0], 0, TETRIS_BOARD_WIDTH);
        cleared++;
        y++;
    }
    return cleared;
}

static tetris_event_t lock_piece(tetris_model_t *model) {
    bool above_board = false;
    for (int py = 0; py < 4; py++) {
        for (int px = 0; px < 4; px++) {
            if (!tetris_piece_cell(model->piece, model->rotation, px, py)) continue;
            int bx = model->x + px;
            int by = model->y + py;
            if (by < 0) {
                above_board = true;
            } else if (bx >= 0 && bx < TETRIS_BOARD_WIDTH && by < TETRIS_BOARD_HEIGHT) {
                model->board[by][bx] = (uint8_t)model->piece;
            }
        }
    }
    if (above_board) {
        model->state = TETRIS_GAME_OVER;
        return TETRIS_EVENT_GAME_OVER;
    }

    int cleared = clear_lines(model);
    if (cleared > 0) {
        static const uint16_t LINE_SCORE[] = { 0, 100, 300, 500, 800 };
        model->score += (uint32_t)LINE_SCORE[cleared] * model->level;
        model->lines += (uint16_t)cleared;
        model->level = (uint8_t)(model->lines / 10U + 1U);
    }
    if (!spawn_piece(model)) return TETRIS_EVENT_GAME_OVER;
    return cleared > 0 ? TETRIS_EVENT_LINE_CLEAR : TETRIS_EVENT_LOCKED;
}

tetris_event_t tetris_model_step(tetris_model_t *model) {
    if (model->state != TETRIS_PLAYING) return TETRIS_EVENT_NONE;
    if (!collides(model, model->piece, model->rotation, model->x, model->y + 1)) {
        model->y++;
        return TETRIS_EVENT_MOVED;
    }
    return lock_piece(model);
}

tetris_event_t tetris_model_hard_drop(tetris_model_t *model) {
    if (model->state != TETRIS_PLAYING) return TETRIS_EVENT_NONE;
    int distance = 0;
    while (!collides(model, model->piece, model->rotation, model->x, model->y + 1)) {
        model->y++;
        distance++;
    }
    model->score += (uint32_t)distance * 2U;
    return lock_piece(model);
}

bool tetris_model_toggle_pause(tetris_model_t *model) {
    if (model->state == TETRIS_PLAYING) {
        model->state = TETRIS_PAUSED;
        return true;
    }
    if (model->state == TETRIS_PAUSED) {
        model->state = TETRIS_PLAYING;
        return true;
    }
    return false;
}

int tetris_model_ghost_y(const tetris_model_t *model) {
    int y = model->y;
    if (model->state == TETRIS_GAME_OVER) return y;
    while (!collides(model, model->piece, model->rotation, model->x, y + 1)) y++;
    return y;
}

uint32_t tetris_model_drop_interval_ms(const tetris_model_t *model) {
    uint32_t reduction = model->level > 1 ? (uint32_t)(model->level - 1U) * 60U : 0;
    return reduction >= 650U ? 150U : 800U - reduction;
}
