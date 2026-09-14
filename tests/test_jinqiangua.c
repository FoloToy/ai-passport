// tests/test_jinqiangua.c
// Host unit test for King Wen Jin Qian Gua logic and 64 hexagrams database.
#include "jinqiangua_logic.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void test_hexagram_data_integrity(void)
{
    printf("Running test_hexagram_data_integrity...\n");
    bool seen_numbers[65] = { false };
    bool seen_codes[64] = { false };

    for (size_t i = 0; i < 64; i++) {
        const jinqiangua_item_t *item = jinqiangua_get_item(i);
        assert(item != NULL);
        assert(item->number >= 1 && item->number <= 64);
        assert(item->code <= 63);
        assert(item->name_pinyin != NULL && strlen(item->name_pinyin) > 0);
        assert(item->name_en != NULL && strlen(item->name_en) > 0);
        assert(item->upper_trigram != NULL);
        assert(item->lower_trigram != NULL);
        assert(item->fortune != NULL);
        assert(item->poem != NULL);

        seen_numbers[item->number] = true;
        seen_codes[item->code] = true;
    }

    for (int n = 1; n <= 64; n++) {
        assert(seen_numbers[n]);
    }
    for (int c = 0; c < 64; c++) {
        assert(seen_codes[c]);
    }

    // Check specific known hexagrams
    const jinqiangua_item_t *qian = jinqiangua_find_by_number(1);
    assert(qian->code == 0x3F); // 111111b
    assert(strcmp(qian->name_pinyin, "Qian Wei Tian") == 0);

    const jinqiangua_item_t *kun = jinqiangua_find_by_number(2);
    assert(kun->code == 0x00); // 000000b
    assert(strcmp(kun->name_pinyin, "Kun Wei Di") == 0);

    const jinqiangua_item_t *tai = jinqiangua_find_by_number(11);
    assert(tai->code == 0x07); // Upper Kun 000, Lower Qian 111 -> 000111b = 7
    assert(strcmp(tai->name_pinyin, "Di Tian Tai") == 0);

    const jinqiangua_item_t *pi = jinqiangua_find_by_number(12);
    assert(pi->code == 0x38); // Upper Qian 111, Lower Kun 000 -> 111000b = 56 (0x38)
    assert(strcmp(pi->name_pinyin, "Tian Di Pi") == 0);

    printf("  -> PASS\n");
}

static void test_session_state_machine(void)
{
    printf("Running test_session_state_machine...\n");
    jinqiangua_session_t session;
    jinqiangua_init(&session);

    assert(session.state == JINQIANGUA_STATE_IDLE);
    assert(session.step == 0);

    jinqiangua_start(&session);
    assert(session.state == JINQIANGUA_STATE_SHAKING);
    assert(session.step == 0);

    // Step 1 to 5
    for (uint32_t i = 0; i < 5; i++) {
        bool finished = jinqiangua_step_shake(&session, 0b111); // 3 heads -> Yang (1)
        assert(!finished);
        assert(session.step == i + 1);
        assert(session.yaos[i] == 1);
        assert(session.state == JINQIANGUA_STATE_SHAKING);
    }

    // Step 6: 3 tails -> 0b000 -> 0 heads -> Yin (0)
    bool finished = jinqiangua_step_shake(&session, 0b000);
    assert(finished);
    assert(session.step == 6);
    assert(session.yaos[5] == 0);
    assert(session.state == JINQIANGUA_STATE_COMPLETED);

    // Code: Yaos 0..4 = 1, Yao 5 = 0 -> 011111b = 31 (0x1F) = Ze Tian Guai (#43)
    assert(session.current_code == 0x1F);
    assert(session.current_gua != NULL);
    assert(session.current_gua->number == 43);

    // Reload
    jinqiangua_reload(&session);
    assert(session.state == JINQIANGUA_STATE_IDLE);
    assert(session.step == 0);

    printf("  -> PASS\n");
}

static void test_browse_navigation(void)
{
    printf("Running test_browse_navigation...\n");
    jinqiangua_session_t session;
    jinqiangua_init(&session);

    jinqiangua_enter_browse(&session);
    assert(session.state == JINQIANGUA_STATE_BROWSE);
    assert(session.browse_index == 0);
    assert(session.current_gua->number == 1);

    jinqiangua_browse_next(&session);
    assert(session.browse_index == 1);
    assert(session.current_gua->number == 2);

    jinqiangua_browse_prev(&session);
    assert(session.browse_index == 0);
    assert(session.current_gua->number == 1);

    // Wrap around to 63
    jinqiangua_browse_prev(&session);
    assert(session.browse_index == 63);
    assert(session.current_gua->number == 64);

    jinqiangua_browse_next(&session);
    assert(session.browse_index == 0);
    assert(session.current_gua->number == 1);

    printf("  -> PASS\n");
}

int main(void)
{
    printf("=== Jin Qian Gua Host Tests ===\n");
    test_hexagram_data_integrity();
    test_session_state_machine();
    test_browse_navigation();
    printf("All Jin Qian Gua host tests passed successfully!\n");
    return 0;
}
