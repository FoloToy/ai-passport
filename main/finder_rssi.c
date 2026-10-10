// main/finder_rssi.c —— 见 finder_rssi.h 的说明。纯 C，无 ESP-IDF 依赖。
#include "finder_rssi.h"

// BLE 接收电平的合理区间；范围外的值不是真实观测，直接丢弃。
#define FINDER_RSSI_MIN_DBM (-120)
#define FINDER_RSSI_MAX_DBM     (0)

// 缓冲区未填满时的处理是刻意的：
//   1 个样本 → 原值
//   2 个样本 → 向下取整的均值（中性；向下取整保证不偏乐观）
//   3 个样本 → 真正的中位数
// 若 2 个样本时取"下中位数"，滤波在前两个样本会完全不动（滞后 0.3–3 秒，取决于
// 对方广播间隔）；若取"上中位数"则电平偏乐观，而偏乐观在这里等于多报"更近"，
// 与 UI 的诚实规则冲突。均值是唯一既不冻结也不夸大的选择。
static int32_t floor_half(int32_t sum)
{
    return sum < 0 ? (int32_t)((sum - 1) / 2) : (int32_t)(sum / 2);
}

static int32_t filtered_of(const int32_t *values, uint8_t count)
{
    if (count >= 3) {
        int32_t a = values[0], b = values[1], c = values[2];
        // 三数取中，无排序数组。
        if ((a <= b && b <= c) || (c <= b && b <= a)) {
            return b;
        }
        if ((b <= a && a <= c) || (c <= a && a <= b)) {
            return a;
        }
        return c;
    }
    if (count == 2) {
        return floor_half(values[0] + values[1]);
    }
    return values[0];
}

void finder_rssi_reset(finder_rssi_t *state)
{
    if (!state) {
        return;
    }
    state->median[0] = 0;
    state->median[1] = 0;
    state->median[2] = 0;
    state->median_count = 0;
    state->median_pos = 0;
    state->has_ema = false;
    state->ema = 0;
    state->last_ms = 0;
    state->has_trend_ref = false;
    state->trend_ref = 0;
    state->trend_ref_ms = 0;
}

void finder_rssi_push(finder_rssi_t *state, int32_t raw_dbm, int64_t now_ms)
{
    if (!state || raw_dbm < FINDER_RSSI_MIN_DBM || raw_dbm > FINDER_RSSI_MAX_DBM) {
        return;
    }

    bool restart = false;
    if (state->has_ema) {
        int64_t dt = now_ms - state->last_ms;
        if (dt <= 0) {
            // dt == 0（同一毫秒的重复上报）或时间倒退：这个样本无法定位在时间轴上。
            return;
        }
        if (dt > FINDER_RSSI_RESET_MS) {
            // 长空洞（例如熄屏、休眠恢复）。跨空洞平滑会把旧电平拖进新场景。
            restart = true;
        }
    } else {
        restart = true;
    }

    if (restart) {
        state->median_count = 0;
        state->median_pos = 0;
        state->has_trend_ref = false;
        state->has_ema = false;
    }

    state->median[state->median_pos] = raw_dbm;
    state->median_pos = (uint8_t)((state->median_pos + 1) % 3);
    if (state->median_count < 3) {
        state->median_count++;
    }

    int32_t filtered = filtered_of(state->median, state->median_count);

    if (!state->has_ema) {
        // 起手：直接置初值，不做平滑（此时没有可用的 dt）。
        state->ema = filtered;
        state->has_ema = true;
    } else {
        int64_t dt = now_ms - state->last_ms;
        // alpha = dt / (tau + dt)，整数运算，先乘后除避免丢精度。
        int64_t delta = (int64_t)(filtered - state->ema) * dt / (FINDER_RSSI_TAU_MS + dt);
        state->ema += (int32_t)delta;
    }

    state->last_ms = now_ms;
    if (!state->has_trend_ref) {
        state->trend_ref = state->ema;
        state->trend_ref_ms = now_ms;
        state->has_trend_ref = true;
    }
}

void finder_rssi_refresh(finder_rssi_t *state, int64_t now_ms)
{
    if (!state || !state->has_ema) {
        return;
    }
    if (!state->has_trend_ref) {
        state->trend_ref = state->ema;
        state->trend_ref_ms = now_ms;
        state->has_trend_ref = true;
        return;
    }
    if (now_ms - state->trend_ref_ms >= FINDER_RSSI_TREND_WINDOW_MS) {
        // 窗口滚动：当前 EMA 成为新的参考点。
        state->trend_ref = state->ema;
        state->trend_ref_ms = now_ms;
    }
}

bool finder_rssi_has_signal(const finder_rssi_t *state, int64_t now_ms)
{
    if (!state || !state->has_ema) {
        return false;
    }
    return (now_ms - state->last_ms) <= FINDER_RSSI_STALE_MS;
}

int32_t finder_rssi_ema(const finder_rssi_t *state)
{
    return state ? state->ema : 0;
}

int8_t finder_rssi_level(const finder_rssi_t *state, int64_t now_ms)
{
    if (!finder_rssi_has_signal(state, now_ms)) {
        return 0;
    }
    int32_t ema = state->ema;
    if (ema <= -90) {
        return 0;
    }
    if (ema <= -80) {
        return 1;
    }
    if (ema <= -70) {
        return 2;
    }
    if (ema <= -60) {
        return 3;
    }
    if (ema <= -50) {
        return 4;
    }
    return 5;
}

finder_trend_t finder_rssi_trend(const finder_rssi_t *state)
{
    if (!state || !state->has_trend_ref || !state->has_ema) {
        return FINDER_TREND_STEADY;
    }
    int32_t diff = state->ema - state->trend_ref;
    if (diff >= FINDER_RSSI_TREND_DB) {
        return FINDER_TREND_STRONGER;
    }
    if (diff <= -FINDER_RSSI_TREND_DB) {
        return FINDER_TREND_WEAKER;
    }
    return FINDER_TREND_STEADY;
}
