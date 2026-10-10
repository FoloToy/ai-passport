// main/finder_table.c —— 见 finder_table.h。纯 C，无 ESP-IDF 依赖。
#include "finder_table.h"

#include <string.h>

// 强度排序键：等级优先，同等级比平滑 RSSI。
// ema 越接近 0 越强（对数为负），所以键越大表示越强。
static int32_t strength_key(int8_t level, int32_t ema)
{
    return (int32_t)level * 10000 + ema;
}

static int32_t entry_strength(const finder_table_t *table, uint8_t i, int64_t now_ms)
{
    const finder_entry_t *e = &table->entry[i];
    return strength_key(finder_rssi_level(&e->rssi, now_ms), finder_rssi_ema(&e->rssi));
}

// 新设备的强度：还没有滤波状态，用首个原始样本近似。
// 阈值与 finder_rssi_level 保持一致，避免两处各写一份导致漂移。
static int8_t raw_level(int32_t raw_dbm)
{
    if (raw_dbm <= -90) {
        return 0;
    }
    if (raw_dbm <= -80) {
        return 1;
    }
    if (raw_dbm <= -70) {
        return 2;
    }
    if (raw_dbm <= -60) {
        return 3;
    }
    if (raw_dbm <= -50) {
        return 4;
    }
    return 5;
}

static void copy_name(char *dst, const char *src)
{
    if (!src || src[0] == '\0') {
        dst[0] = '\0';
        return;
    }
    size_t n = strlen(src);
    if (n > FINDER_NAME_MAX) {
        n = FINDER_NAME_MAX;
    }
    memcpy(dst, src, n);
    dst[n] = '\0';
}

uint16_t finder_table_angle_for(const uint8_t addr[6])
{
    // FNV-1a：同一地址在本次会话内总是得到同一角度，目的是让用户能盯住同一个点，
    // 不是为了指示方向。
    uint32_t hash = 2166136261u;
    for (uint8_t i = 0; i < 6; i++) {
        hash ^= addr[i];
        hash *= 16777619u;
    }
    return (uint16_t)(hash % 360u);
}

void finder_table_reset(finder_table_t *table)
{
    if (!table) {
        return;
    }
    memset(table, 0, sizeof(*table));
    for (uint8_t i = 0; i < FINDER_TABLE_MAX; i++) {
        finder_rssi_reset(&table->entry[i].rssi);
    }
}

static uint8_t find_by_addr(const finder_table_t *table, const uint8_t addr[6])
{
    for (uint8_t i = 0; i < FINDER_TABLE_MAX; i++) {
        if (table->entry[i].used && memcmp(table->entry[i].addr, addr, 6) == 0) {
            return i;
        }
    }
    return FINDER_TABLE_MAX;
}

static uint8_t find_free(const finder_table_t *table)
{
    for (uint8_t i = 0; i < FINDER_TABLE_MAX; i++) {
        if (!table->entry[i].used) {
            return i;
        }
    }
    return FINDER_TABLE_MAX;
}

static uint8_t find_weakest(const finder_table_t *table, int64_t now_ms)
{
    uint8_t weakest = 0;
    int32_t weakest_key = entry_strength(table, 0, now_ms);
    for (uint8_t i = 1; i < FINDER_TABLE_MAX; i++) {
        int32_t key = entry_strength(table, i, now_ms);
        if (key < weakest_key) {
            weakest_key = key;
            weakest = i;
        }
    }
    return weakest;
}

// 名称回认：地址轮换后同一台设备会以新地址出现。若旧条目已安静了一会儿且名字相同，
// 就把它重新绑定到新地址，从而继承原角度与滤波状态。
// 已知风险：不同设备可能广播相同名字（例如出厂默认名），此时会误并两台设备。
// FINDER_REASSOC_QUIET_MS 与"名字必须非空"是压低该风险的两道限制。
static uint8_t find_reassoc(const finder_table_t *table, const char *name, int64_t now_ms)
{
    if (!name || name[0] == '\0') {
        return FINDER_TABLE_MAX;
    }
    for (uint8_t i = 0; i < FINDER_TABLE_MAX; i++) {
        const finder_entry_t *e = &table->entry[i];
        if (!e->used || e->name[0] == '\0') {
            continue;
        }
        if (strcmp(e->name, name) != 0) {
            continue;
        }
        if (now_ms - e->last_ms < FINDER_REASSOC_QUIET_MS) {
            continue; // 它还在活跃广播，不是"刚换了地址"
        }
        return i;
    }
    return FINDER_TABLE_MAX;
}

int8_t finder_table_observe(finder_table_t *table, const uint8_t addr[6],
                            const char *name, int32_t raw_dbm, int64_t now_ms)
{
    if (!table || !addr) {
        return -1;
    }

    uint8_t slot = find_by_addr(table, addr);
    if (slot < FINDER_TABLE_MAX) {
        finder_entry_t *e = &table->entry[slot];
        if (name && name[0] != '\0') {
            copy_name(e->name, name);
        }
        finder_rssi_push(&e->rssi, raw_dbm, now_ms);
        e->last_ms = now_ms;
        return (int8_t)slot;
    }

    slot = find_reassoc(table, name, now_ms);
    if (slot < FINDER_TABLE_MAX) {
        // 同一台设备换了地址：继承角度，滤波状态继续沿用（不重置）。
        finder_entry_t *e = &table->entry[slot];
        memcpy(e->addr, addr, 6);
        finder_rssi_push(&e->rssi, raw_dbm, now_ms);
        e->last_ms = now_ms;
        return (int8_t)slot;
    }

    int32_t newcomer_key = strength_key(raw_level(raw_dbm), raw_dbm);

    slot = find_free(table);
    if (slot == FINDER_TABLE_MAX) {
        // 表满：只有当新设备不弱于当前最弱项时才顶替它。
        uint8_t weakest = find_weakest(table, now_ms);
        if (newcomer_key <= entry_strength(table, weakest, now_ms)) {
            return -1;
        }
        slot = weakest;
    }

    finder_entry_t *e = &table->entry[slot];
    memset(e, 0, sizeof(*e));
    memcpy(e->addr, addr, 6);
    copy_name(e->name, name);
    e->used = true;
    e->angle_deg = finder_table_angle_for(addr);
    finder_rssi_reset(&e->rssi);
    finder_rssi_push(&e->rssi, raw_dbm, now_ms);
    e->last_ms = now_ms;
    return (int8_t)slot;
}

void finder_table_refresh(finder_table_t *table, int64_t now_ms)
{
    if (!table) {
        return;
    }
    for (uint8_t i = 0; i < FINDER_TABLE_MAX; i++) {
        finder_entry_t *e = &table->entry[i];
        if (!e->used) {
            continue;
        }
        finder_rssi_refresh(&e->rssi, now_ms);
        if (now_ms - e->last_ms > FINDER_AGE_MS) {
            // 老化移除：旧点凭空消失好过让一个已经离开的设备继续占据视野。
            memset(e, 0, sizeof(*e));
            finder_rssi_reset(&e->rssi);
        }
    }
}

uint8_t finder_table_count(const finder_table_t *table)
{
    if (!table) {
        return 0;
    }
    uint8_t n = 0;
    for (uint8_t i = 0; i < FINDER_TABLE_MAX; i++) {
        if (table->entry[i].used) {
            n++;
        }
    }
    return n;
}

uint8_t finder_table_ranked(const finder_table_t *table, int64_t now_ms,
                            int8_t *out_order, uint8_t cap)
{
    if (!table || !out_order || cap == 0) {
        return 0;
    }

    int8_t order[FINDER_TABLE_MAX];
    int32_t keys[FINDER_TABLE_MAX];
    uint8_t n = 0;
    for (uint8_t i = 0; i < FINDER_TABLE_MAX; i++) {
        if (!table->entry[i].used) {
            continue;
        }
        int32_t key = entry_strength(table, i, now_ms);
        uint8_t j = n;
        while (j > 0 && keys[j - 1] < key) { // 降序：强的在前
            order[j] = order[j - 1];
            keys[j] = keys[j - 1];
            j--;
        }
        order[j] = (int8_t)i;
        keys[j] = key;
        n++;
    }

    if (n > cap) {
        n = cap;
    }
    memcpy(out_order, order, n);
    return n;
}

int8_t finder_table_next(const finder_table_t *table, int64_t now_ms, int8_t index)
{
    int8_t order[FINDER_TABLE_MAX];
    uint8_t n = finder_table_ranked(table, now_ms, order, FINDER_TABLE_MAX);
    if (n == 0) {
        return -1;
    }
    for (uint8_t i = 0; i < n; i++) {
        if (order[i] == index) {
            return order[(uint8_t)((i + 1) % n)];
        }
    }
    return order[0]; // index 已不在表中（老化或被顶替）：回到最强项
}
