// main/finder_table.h —— 附近设备表（纯逻辑，可宿主测试）。
//
// 责任：按【会话地址】维护最多 FINDER_TABLE_MAX 台设备，每台内嵌自己的 RSSI 滤波状态；
// 处理插入/更新、超限淘汰、老化移除、会话内固定角度、以及"下一个"所需的强弱排序。
//
// 明确不做身份匹配：这里的"地址"只是本次会话内观察到的广播地址，不写 NVS、不跨会话复用。
// 空口地址约每 15 分钟轮换（见 PRD §2），因此表中条目会以新地址重现——name_reassociate
// 提供一层保守的回认，但很多设备既无名字也无稳定标识，只能接受。
//
// 本文件不得包含 ESP-IDF / LVGL 头文件。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "finder_rssi.h"

#define FINDER_TABLE_MAX     6   // 同屏上限（无 PSRAM，见 PRD §10.4）
#define FINDER_NAME_MAX     24   // 存储的广播名上限，超出截断
#define FINDER_AGE_MS     5000   // 超过此时长未见 → 从表中移除（可标定）
#define FINDER_REASSOC_QUIET_MS 1000 // 名字回认要求旧条目已安静这么久，降低误并

typedef struct {
    uint8_t       addr[6];
    bool          used;
    char          name[FINDER_NAME_MAX + 1]; // "" 表示无名字
    finder_rssi_t rssi;
    int64_t       last_ms;                   // 最近一次观测到的时间
    uint16_t      angle_deg;                 // 会话内固定角度（装饰性，不承载信息）
} finder_entry_t;

typedef struct {
    finder_entry_t entry[FINDER_TABLE_MAX];
} finder_table_t;

void finder_table_reset(finder_table_t *table);

// 观测到一台设备。已存在则更新，不存在则插入。
// 表满且新设备不弱于最弱项时淘汰最弱项；新设备更弱则返回 -1（不进入视野）。
// name 可为 NULL 或空串，表示无名。
int8_t finder_table_observe(finder_table_t *table, const uint8_t addr[6],
                            const char *name, int32_t raw_dbm, int64_t now_ms);

// 周期性调用：推进每台设备的趋势窗口、移除老化条目。
void finder_table_refresh(finder_table_t *table, int64_t now_ms);

uint8_t finder_table_count(const finder_table_t *table);

// 按 等级（高→低）→ 平滑 RSSI（强→弱）排序，把条目下标写入 out_order。
// 返回实际写入数量（不超过 cap 与当前条目数）。
uint8_t finder_table_ranked(const finder_table_t *table, int64_t now_ms,
                            int8_t *out_order, uint8_t cap);

// 会话内固定角度：由地址哈希得到 0..359。仅用于视觉稳定，不承载方向信息。
uint16_t finder_table_angle_for(const uint8_t addr[6]);

// 在排序序列中找到 index 的下一个（末尾回绕到第一个）。表中只有它自己时返回 index。
int8_t finder_table_next(const finder_table_t *table, int64_t now_ms, int8_t index);
