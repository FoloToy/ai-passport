// main/jinqiangua_logic.h
// Core mathematical logic, coin toss, and state machine for King Wen Jin Qian Gua.
#pragma once

#include "jinqiangua_data.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    JINQIANGUA_STATE_IDLE = 0,       // Idle: waiting for user to start
    JINQIANGUA_STATE_SHAKING,        // Shaking: tossing coins (step 1..6)
    JINQIANGUA_STATE_COMPLETED,      // Completed: hexagram formed, showing result
    JINQIANGUA_STATE_BROWSE,         // Browse mode: browsing 64 hexagrams
} jinqiangua_state_t;

typedef struct {
    jinqiangua_state_t state;
    uint8_t step;                    // 0..6
    uint8_t yaos[6];                 // 0 = Yin (⚋), 1 = Yang (⚊). yaos[0]=bottom, yaos[5]=top
    uint8_t coins[3];                // Last 3 coins (0 = tail/yin, 1 = head/yang)
    uint8_t current_code;            // 6-bit code
    const jinqiangua_item_t *current_gua;
    size_t browse_index;             // 0..63
} jinqiangua_session_t;

void jinqiangua_init(jinqiangua_session_t *session);
void jinqiangua_start(jinqiangua_session_t *session);

// Perform one toss step using random_val (random bits).
// Returns true when all 6 steps are finished.
bool jinqiangua_step_shake(jinqiangua_session_t *session, uint32_t random_val);

void jinqiangua_reload(jinqiangua_session_t *session);
void jinqiangua_enter_browse(jinqiangua_session_t *session);
void jinqiangua_browse_prev(jinqiangua_session_t *session);
void jinqiangua_browse_next(jinqiangua_session_t *session);

const jinqiangua_item_t *jinqiangua_find_by_code(uint8_t code);
const jinqiangua_item_t *jinqiangua_find_by_number(uint8_t number);
const jinqiangua_item_t *jinqiangua_get_item(size_t index);

#ifdef __cplusplus
}
#endif
