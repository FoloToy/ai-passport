// main/jinqiangua_logic.c
#include "jinqiangua_logic.h"
#include <string.h>

void jinqiangua_init(jinqiangua_session_t *session)
{
    if (!session) return;
    memset(session, 0, sizeof(*session));
    session->state = JINQIANGUA_STATE_IDLE;
    session->step = 0;
    session->browse_index = 0;
    session->current_gua = &G_JINQIANGUA_64[0];
}

void jinqiangua_start(jinqiangua_session_t *session)
{
    if (!session) return;
    session->state = JINQIANGUA_STATE_SHAKING;
    session->step = 0;
    session->current_code = 0;
    session->current_gua = NULL;
    memset(session->yaos, 0, sizeof(session->yaos));
    memset(session->coins, 0, sizeof(session->coins));
}

bool jinqiangua_step_shake(jinqiangua_session_t *session, uint32_t random_val)
{
    if (!session || session->step >= 6) return true;

    // Simulate 3 coins being tossed
    // 0 = Tail (字/阴), 1 = Head (背/阳)
    session->coins[0] = (random_val >> 0) & 1;
    session->coins[1] = (random_val >> 1) & 1;
    session->coins[2] = (random_val >> 2) & 1;

    uint8_t heads = session->coins[0] + session->coins[1] + session->coins[2];

    // Traditional Jin Qian Gua rule:
    // 1 Head (单背) or 3 Heads (三背) -> Yang (1)
    // 0 Heads (三字) or 2 Heads (二背) -> Yin (0)
    uint8_t yao = (heads % 2 == 1) ? 1 : 0;
    session->yaos[session->step] = yao;
    session->step++;

    if (session->step >= 6) {
        uint8_t code = 0;
        for (int i = 0; i < 6; i++) {
            if (session->yaos[i]) {
                code |= (1 << i);
            }
        }
        session->current_code = code;
        session->current_gua = jinqiangua_find_by_code(code);
        session->state = JINQIANGUA_STATE_COMPLETED;
        return true;
    }

    return false;
}

void jinqiangua_reload(jinqiangua_session_t *session)
{
    jinqiangua_init(session);
}

void jinqiangua_enter_browse(jinqiangua_session_t *session)
{
    if (!session) return;
    session->state = JINQIANGUA_STATE_BROWSE;
    session->browse_index = 0;
    session->current_gua = &G_JINQIANGUA_64[0];
    session->current_code = session->current_gua->code;
    for (int i = 0; i < 6; i++) {
        session->yaos[i] = (session->current_code >> i) & 1;
    }
}

void jinqiangua_browse_prev(jinqiangua_session_t *session)
{
    if (!session) return;
    if (session->browse_index == 0) {
        session->browse_index = 63;
    } else {
        session->browse_index--;
    }
    session->current_gua = &G_JINQIANGUA_64[session->browse_index];
    session->current_code = session->current_gua->code;
    for (int i = 0; i < 6; i++) {
        session->yaos[i] = (session->current_code >> i) & 1;
    }
}

void jinqiangua_browse_next(jinqiangua_session_t *session)
{
    if (!session) return;
    session->browse_index = (session->browse_index + 1) % 64;
    session->current_gua = &G_JINQIANGUA_64[session->browse_index];
    session->current_code = session->current_gua->code;
    for (int i = 0; i < 6; i++) {
        session->yaos[i] = (session->current_code >> i) & 1;
    }
}

const jinqiangua_item_t *jinqiangua_find_by_code(uint8_t code)
{
    uint8_t target = code & 0x3F;
    for (size_t i = 0; i < 64; i++) {
        if (G_JINQIANGUA_64[i].code == target) {
            return &G_JINQIANGUA_64[i];
        }
    }
    return &G_JINQIANGUA_64[0];
}

const jinqiangua_item_t *jinqiangua_find_by_number(uint8_t number)
{
    if (number >= 1 && number <= 64) {
        return &G_JINQIANGUA_64[number - 1];
    }
    return &G_JINQIANGUA_64[0];
}

const jinqiangua_item_t *jinqiangua_get_item(size_t index)
{
    if (index < 64) {
        return &G_JINQIANGUA_64[index];
    }
    return &G_JINQIANGUA_64[0];
}
