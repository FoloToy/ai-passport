// tests/test_finder_rssi.c —— finder_rssi 的宿主测试（不需要 ESP-IDF）。
#include <assert.h>
#include <stdio.h>

#include "finder_rssi.h"

#define T0 1000000

static finder_rssi_t g;

static int8_t level_of(int32_t dbm)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, dbm, T0);
    return finder_rssi_level(&g, T0);
}

static void test_reset_has_no_signal(void)
{
    finder_rssi_reset(&g);
    assert(!finder_rssi_has_signal(&g, T0));
    assert(finder_rssi_level(&g, T0) == 0);
    assert(finder_rssi_trend(&g) == FINDER_TREND_STEADY);
}

static void test_first_sample_sets_ema_without_smoothing(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -60, T0);
    assert(finder_rssi_has_signal(&g, T0));
    assert(finder_rssi_ema(&g) == -60);
}

// 等级边界必须逐一钉住：这几条阈值是 UI 文案的唯一输入。
static void test_level_boundaries(void)
{
    assert(level_of(-95) == 0);
    assert(level_of(-90) == 0); // -90 归 0（保守：宁可说没有信号）
    assert(level_of(-89) == 1);
    assert(level_of(-80) == 1);
    assert(level_of(-79) == 2);
    assert(level_of(-70) == 2);
    assert(level_of(-69) == 3);
    assert(level_of(-60) == 3);
    assert(level_of(-59) == 4);
    assert(level_of(-50) == 4);
    assert(level_of(-49) == 5);
}

static void test_out_of_range_samples_are_dropped(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -80, T0);
    finder_rssi_push(&g, -200, T0 + 100); // 低于物理合理下限
    finder_rssi_push(&g, 5, T0 + 200);    // 高于物理合理上限
    assert(finder_rssi_ema(&g) == -80);
    assert(finder_rssi_level(&g, T0 + 200) == 1);
}

static void test_same_millisecond_sample_is_ignored(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -80, T0);
    finder_rssi_push(&g, -30, T0); // dt == 0：无法定位在时间轴上
    assert(finder_rssi_ema(&g) == -80);
}

static void test_time_weighted_smoothing(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -80, T0);
    // 第二个样本：平滑输入是 floor_mean(-80,-40) = -60；
    // alpha = 1000/(2000+1000)，delta = 20*1000/3000 = 6
    finder_rssi_push(&g, -40, T0 + 1000);
    assert(finder_rssi_ema(&g) == -74);
    // 第三个样本：缓冲填满，平滑输入变成真正的中位数 -40；
    // delta = (-40 - -74)*1000/3000 = 11
    finder_rssi_push(&g, -40, T0 + 2000);
    assert(finder_rssi_ema(&g) == -63);
}

// 两个样本时用均值而不是上中位数：上中位数会让电平偏乐观，等于多报"更近"。
static void test_two_sample_input_is_mean_not_upper_median(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -80, T0);
    finder_rssi_push(&g, -40, T0 + 1000);
    assert(finder_rssi_ema(&g) == -74); // 上中位数会得到 -67
}

// 均值向下取整：奇数和的截断会偏乐观一档，这里钉住保守方向。
static void test_two_sample_mean_rounds_away_from_optimism(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -81, T0);
    finder_rssi_push(&g, -40, T0 + 1000);
    assert(finder_rssi_ema(&g) == -75); // 向零截断会得到 -74
}

static void test_large_gap_restarts_instead_of_smoothing(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -80, T0);
    finder_rssi_push(&g, -40, T0 + 20000); // 超过 RESET_MS：跨空洞平滑会把旧场景拖进来
    assert(finder_rssi_ema(&g) == -40);
    assert(finder_rssi_level(&g, T0 + 20000) == 5);
}

static void test_stale_signal_decays_to_level_zero(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -40, T0);
    assert(finder_rssi_level(&g, T0) == 5);
    assert(finder_rssi_level(&g, T0 + FINDER_RSSI_STALE_MS) == 5); // 边界上仍算有信号
    assert(finder_rssi_level(&g, T0 + FINDER_RSSI_STALE_MS + 1) == 0);
    assert(!finder_rssi_has_signal(&g, T0 + FINDER_RSSI_STALE_MS + 1));
}

static void test_trend_needs_hysteresis(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -60, T0);
    finder_rssi_refresh(&g, T0 + FINDER_RSSI_TREND_WINDOW_MS + 1); // 参考点固定为 -60
    finder_rssi_push(&g, -58, T0 + 6000);                          // 移动不足 3dB
    assert(finder_rssi_trend(&g) == FINDER_TREND_STEADY);
}

static void test_trend_reports_stronger_and_weaker(void)
{
    // 注意：上升过程中【不能】调 refresh。趋势参考窗口每 5 秒滚动一次，
    // 若每步都刷新，参考点会一路追上来，把"持续上升"读成"稳态"——那是正确语义
    // （趋势回答的是"此刻相比 5 秒前是否更强"），但会让这个测试测不到东西。
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -80, T0);
    finder_rssi_refresh(&g, T0 + FINDER_RSSI_TREND_WINDOW_MS + 1); // 参考点固定在 -80
    for (int i = 1; i <= 4; i++) {
        finder_rssi_push(&g, -45, T0 + 5000 + i * 1000);
    }
    assert(finder_rssi_ema(&g) == -53);
    assert(finder_rssi_trend(&g) == FINDER_TREND_STRONGER);

    finder_rssi_reset(&g);
    finder_rssi_push(&g, -45, T0);
    finder_rssi_refresh(&g, T0 + FINDER_RSSI_TREND_WINDOW_MS + 1); // 参考点固定在 -45
    for (int i = 1; i <= 4; i++) {
        finder_rssi_push(&g, -85, T0 + 5000 + i * 1000);
    }
    assert(finder_rssi_ema(&g) == -77);
    assert(finder_rssi_trend(&g) == FINDER_TREND_WEAKER);
}

// 持续 5 秒以上的变化会被参考窗口吸收，回到稳态。这是刻意语义，钉住防止误改。
static void test_sustained_change_becomes_steady_after_window(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -80, T0);
    finder_rssi_refresh(&g, T0 + FINDER_RSSI_TREND_WINDOW_MS + 1); // 参考点固定在 -80
    for (int i = 1; i <= 4; i++) {
        finder_rssi_push(&g, -45, T0 + 5000 + i * 1000);
    }
    // 此刻仍是"更强"
    assert(finder_rssi_trend(&g) == FINDER_TREND_STRONGER);

    // 让参考窗口滚过最新样本：参考点追平当前电平，趋势回到稳态。
    finder_rssi_refresh(&g, T0 + 9000 + FINDER_RSSI_TREND_WINDOW_MS);
    assert(finder_rssi_trend(&g) == FINDER_TREND_STEADY);
}

// 空洞之后趋势参考必须重新起算，否则"5 秒前"指的是上一个场景。
static void test_trend_reference_resets_after_gap(void)
{
    finder_rssi_reset(&g);
    finder_rssi_push(&g, -85, T0);
    finder_rssi_push(&g, -40, T0 + 20000); // 跨空洞重启
    assert(finder_rssi_trend(&g) == FINDER_TREND_STEADY);
}

int main(void)
{
    test_reset_has_no_signal();
    test_first_sample_sets_ema_without_smoothing();
    test_level_boundaries();
    test_out_of_range_samples_are_dropped();
    test_same_millisecond_sample_is_ignored();
    test_time_weighted_smoothing();
    test_two_sample_input_is_mean_not_upper_median();
    test_two_sample_mean_rounds_away_from_optimism();
    test_large_gap_restarts_instead_of_smoothing();
    test_stale_signal_decays_to_level_zero();
    test_trend_needs_hysteresis();
    test_trend_reports_stronger_and_weaker();
    test_sustained_change_becomes_steady_after_window();
    test_trend_reference_resets_after_gap();

    printf("test_finder_rssi: all assertions passed\n");
    return 0;
}
