// tests/test_finder_table.c —— finder_table 的宿主测试（不需要 ESP-IDF）。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "finder_table.h"

#define T0 1000000

static finder_table_t g;

static void addr_of(uint8_t *addr, uint8_t id)
{
    memset(addr, 0, 6);
    addr[5] = id;
}

static int8_t observe(uint8_t id, const char *name, int32_t dbm, int64_t now_ms)
{
    uint8_t addr[6];
    addr_of(addr, id);
    return finder_table_observe(&g, addr, name, dbm, now_ms);
}

static void test_reset_is_empty(void)
{
    finder_table_reset(&g);
    assert(finder_table_count(&g) == 0);
    int8_t order[FINDER_TABLE_MAX];
    assert(finder_table_ranked(&g, T0, order, FINDER_TABLE_MAX) == 0);
}

static void test_observe_inserts_then_updates_in_place(void)
{
    finder_table_reset(&g);
    int8_t i0 = observe(1, "KEY-TAG", -70, T0);
    assert(i0 == 0);
    assert(finder_table_count(&g) == 1);
    assert(strcmp(g.entry[0].name, "KEY-TAG") == 0);
    assert(g.entry[0].angle_deg < 360);

    uint16_t angle = g.entry[0].angle_deg;
    int8_t i1 = observe(1, "KEY-TAG", -60, T0 + 1000);
    assert(i1 == i0); // 同一地址必须原地更新，不得新增条目
    assert(finder_table_count(&g) == 1);
    assert(g.entry[0].angle_deg == angle); // 会话内角度稳定
}

static void test_unnamed_device_gets_empty_name(void)
{
    finder_table_reset(&g);
    assert(observe(2, NULL, -70, T0) == 0);
    assert(g.entry[0].name[0] == '\0');
}

static void test_long_name_is_truncated(void)
{
    finder_table_reset(&g);
    static const char *long_name = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    assert(observe(3, long_name, -70, T0) == 0);
    assert(strlen(g.entry[0].name) == FINDER_NAME_MAX);
}

static void test_weaker_newcomer_is_rejected_when_full(void)
{
    finder_table_reset(&g);
    for (uint8_t id = 1; id <= FINDER_TABLE_MAX; id++) {
        assert(observe(id, NULL, -80, T0) == (int8_t)(id - 1));
    }
    assert(finder_table_count(&g) == FINDER_TABLE_MAX);

    // 满表 + 明显更弱的设备 → 不进入视野
    assert(observe(99, NULL, -95, T0 + 100) == -1);
    assert(finder_table_count(&g) == FINDER_TABLE_MAX);

    uint8_t addr[6];
    addr_of(addr, 99);
    for (uint8_t i = 0; i < FINDER_TABLE_MAX; i++) {
        assert(memcmp(g.entry[i].addr, addr, 6) != 0);
    }
}

static void test_stronger_newcomer_evicts_weakest(void)
{
    finder_table_reset(&g);
    for (uint8_t id = 1; id <= FINDER_TABLE_MAX; id++) {
        observe(id, NULL, -80, T0);
    }
    int8_t slot = observe(99, "NEW", -45, T0 + 100);
    assert(slot >= 0);
    assert(slot < FINDER_TABLE_MAX);
    assert(finder_table_count(&g) == FINDER_TABLE_MAX); // 顶替，不是新增

    uint8_t addr[6];
    addr_of(addr, 99);
    assert(memcmp(g.entry[slot].addr, addr, 6) == 0);
    assert(strcmp(g.entry[slot].name, "NEW") == 0);
    assert(finder_rssi_level(&g.entry[slot].rssi, T0 + 100) == 5);
}

static void test_aging_removes_quiet_entries(void)
{
    finder_table_reset(&g);
    observe(1, NULL, -60, T0);
    finder_table_refresh(&g, T0 + FINDER_AGE_MS);
    assert(finder_table_count(&g) == 1); // 边界上仍保留
    finder_table_refresh(&g, T0 + FINDER_AGE_MS + 1);
    assert(finder_table_count(&g) == 0); // 超时移除
}

static void test_ranked_is_strongest_first(void)
{
    finder_table_reset(&g);
    observe(1, "A", -85, T0); // 等级 1
    observe(2, "B", -65, T0); // 等级 3
    observe(3, "C", -45, T0); // 等级 5

    int8_t order[FINDER_TABLE_MAX];
    uint8_t n = finder_table_ranked(&g, T0, order, FINDER_TABLE_MAX);
    assert(n == 3);
    assert(order[0] == 2); // C 最强
    assert(order[1] == 1); // B
    assert(order[2] == 0); // A 最弱
}

static void test_ranked_respects_capacity(void)
{
    finder_table_reset(&g);
    for (uint8_t id = 1; id <= FINDER_TABLE_MAX; id++) {
        observe(id, NULL, -60 - id, T0);
    }
    int8_t order[2];
    assert(finder_table_ranked(&g, T0, order, 2) == 2);
}

static void test_next_cycles_and_wraps(void)
{
    finder_table_reset(&g);
    observe(1, "A", -85, T0);
    observe(2, "B", -65, T0);
    observe(3, "C", -45, T0);

    assert(finder_table_next(&g, T0, 2) == 1); // C → B
    assert(finder_table_next(&g, T0, 1) == 0); // B → A
    assert(finder_table_next(&g, T0, 0) == 2); // A → 回绕到 C
}

static void test_next_with_single_entry_stays_put(void)
{
    finder_table_reset(&g);
    observe(1, "ONLY", -60, T0);
    assert(finder_table_next(&g, T0, 0) == 0);
}

// 地址轮换（约每 15 分钟）后同一台设备会以新地址出现。名字相同且旧条目已安静时，
// 回认到原条目，从而继承角度与滤波状态，避免"同一个东西变成两个点"。
static void test_quiet_name_match_reassociates_address(void)
{
    finder_table_reset(&g);
    observe(1, "MY-TAG", -70, T0);
    uint16_t angle = g.entry[0].angle_deg;

    int8_t slot = observe(2, "MY-TAG", -68, T0 + FINDER_REASSOC_QUIET_MS + 1);
    assert(slot == 0);
    assert(finder_table_count(&g) == 1); // 仍是一台设备
    assert(g.entry[0].angle_deg == angle);

    uint8_t addr[6];
    addr_of(addr, 2);
    assert(memcmp(g.entry[0].addr, addr, 6) == 0);
}

// 还在活跃广播的同名设备不得被回认吸收（那是两台同时在场的设备）。
static void test_active_name_match_does_not_reassociate(void)
{
    finder_table_reset(&g);
    observe(1, "SAME", -70, T0);
    int8_t slot = observe(2, "SAME", -70, T0 + 100);
    assert(slot == 1);
    assert(finder_table_count(&g) == 2);
}

// 无名字的设备无法回认，只能作为新条目出现（R11 的已知残留）。
static void test_unnamed_rotation_creates_second_entry(void)
{
    finder_table_reset(&g);
    observe(1, NULL, -70, T0);
    observe(2, NULL, -70, T0 + FINDER_REASSOC_QUIET_MS + 1);
    assert(finder_table_count(&g) == 2);
}

static void test_different_names_do_not_reassociate(void)
{
    finder_table_reset(&g);
    observe(1, "AAA", -70, T0);
    observe(2, "BBB", -70, T0 + FINDER_REASSOC_QUIET_MS + 1);
    assert(finder_table_count(&g) == 2);
}

int main(void)
{
    test_reset_is_empty();
    test_observe_inserts_then_updates_in_place();
    test_unnamed_device_gets_empty_name();
    test_long_name_is_truncated();
    test_weaker_newcomer_is_rejected_when_full();
    test_stronger_newcomer_evicts_weakest();
    test_aging_removes_quiet_entries();
    test_ranked_is_strongest_first();
    test_ranked_respects_capacity();
    test_next_cycles_and_wraps();
    test_next_with_single_entry_stays_put();
    test_quiet_name_match_reassociates_address();
    test_active_name_match_does_not_reassociate();
    test_unnamed_rotation_creates_second_entry();
    test_different_names_do_not_reassociate();

    printf("test_finder_table: all assertions passed\n");
    return 0;
}
