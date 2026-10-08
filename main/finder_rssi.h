// main/finder_rssi.h —— 单设备的 RSSI 平滑、等级与趋势判定（纯逻辑，可宿主测试）。
//
// 采样节奏由【被探测设备】的广播间隔决定（0.1–2 s），且中间常有空洞，
// 因此必须按【经过时间】给平滑计权，不能按样本数计权。
// 处理链：raw → median-of-3（去尖峰）→ dt 加权 EMA（真正平滑）。
//
// 本文件不得包含 ESP-IDF / LVGL 头文件：它必须能在宿主机上用 cc 直接编译。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FINDER_RSSI_TAU_MS           2000  // EMA 时间常数 τ
#define FINDER_RSSI_RESET_MS        10000  // 空洞超过此值 → 当作重新起手，不跨空洞平滑
#define FINDER_RSSI_STALE_MS         5000  // 超过此值没有新样本 → 视为无信号（等级 0）
#define FINDER_RSSI_TREND_WINDOW_MS  5000  // 趋势参考点：约 5 秒前的 EMA
#define FINDER_RSSI_TREND_DB            3  // 趋势箭头移动所需的迟滞

typedef enum {
    FINDER_TREND_WEAKER   = -1,
    FINDER_TREND_STEADY   =  0,
    FINDER_TREND_STRONGER =  1,
} finder_trend_t;

typedef struct {
    int32_t median[3];      // median-of-3 环形缓冲
    uint8_t median_count;   // 已填入的样本数（0..3）
    uint8_t median_pos;
    bool    has_ema;
    int32_t ema;
    int64_t last_ms;
    bool    has_trend_ref;
    int32_t trend_ref;      // 趋势参考：约 TREND_WINDOW 之前的 EMA
    int64_t trend_ref_ms;
} finder_rssi_t;

// 清空状态。首次使用前必须调用一次。
void finder_rssi_reset(finder_rssi_t *state);

// 喂入一个原始 RSSI 样本。越界值（|raw| 明显超出 BLE 合理范围）会被丢弃。
void finder_rssi_push(finder_rssi_t *state, int32_t raw_dbm, int64_t now_ms);

// 周期性调用（每个 UI tick 一次）：推进趋势参考窗口。
// 它不产生新样本，只负责让"5 秒前"这个参照随时间滚动。
void finder_rssi_refresh(finder_rssi_t *state, int64_t now_ms);

// 以下三个为纯查询，不修改状态。
bool            finder_rssi_has_signal(const finder_rssi_t *state, int64_t now_ms);
int32_t         finder_rssi_ema(const finder_rssi_t *state);
int8_t          finder_rssi_level(const finder_rssi_t *state, int64_t now_ms);
finder_trend_t  finder_rssi_trend(const finder_rssi_t *state);
