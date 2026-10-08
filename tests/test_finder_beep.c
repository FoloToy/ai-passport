// tests/test_finder_beep.c —— finder_beep 的宿主测试（不需要 ESP-IDF）。
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "finder_beep.h"

#define RATE FINDER_BEEP_RATE_HZ

static finder_beep_gen_t g;
static int16_t buf[2048];
static int16_t buf2[2048];

static void test_cadence_table(void)
{
    assert(finder_beep_interval_ms(0) == 0);
    assert(finder_beep_interval_ms(1) == 2500);
    assert(finder_beep_interval_ms(2) == 1000);
    assert(finder_beep_interval_ms(3) == 500);
    assert(finder_beep_interval_ms(4) == 200);
    assert(finder_beep_interval_ms(5) == 80);
    // 越界等级必须收敛到"静默"，不能越界读表
    assert(finder_beep_interval_ms(-1) == 0);
    assert(finder_beep_interval_ms(9) == 0);

    assert(finder_beep_tone_hz(0) == 0);
    assert(finder_beep_tone_hz(5) == 3000);
    assert(finder_beep_tone_hz(9) == 0);
}

static int all_zero(const int16_t *samples, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        if (samples[i] != 0) {
            return 0;
        }
    }
    return 1;
}

static int count_sign_flips(const int16_t *samples, uint32_t n)
{
    int flips = 0;
    for (uint32_t i = 1; i < n; i++) {
        if ((samples[i - 1] > 0 && samples[i] < 0) || (samples[i - 1] < 0 && samples[i] > 0)) {
            flips++;
        }
    }
    return flips;
}

static void test_level_zero_is_silent(void)
{
    finder_beep_init(&g, RATE);
    finder_beep_set(&g, 0, false);
    assert(finder_beep_next(&g, buf, 256) == 256);
    assert(all_zero(buf, 256));
}

static void test_mute_is_silent(void)
{
    finder_beep_init(&g, RATE);
    finder_beep_set(&g, 5, true);
    assert(finder_beep_next(&g, buf, 256) == 256);
    assert(all_zero(buf, 256));
}

// 等级 5：周期 80ms = 1280 样本，其中前 40ms = 640 样本发声，其余静默。
static void test_level5_cycle_shape(void)
{
    finder_beep_init(&g, RATE);
    finder_beep_set(&g, 5, false);
    assert(finder_beep_next(&g, buf, 1280) == 1280);

    int nonzero_in_tone = 0;
    for (uint32_t i = 0; i < 640; i++) {
        if (buf[i] != 0) {
            nonzero_in_tone++;
        }
    }
    assert(nonzero_in_tone > 500);
    assert(all_zero(buf + 640, 640)); // 后半段必须是静默
}

static void test_amplitude_stays_within_peak(void)
{
    finder_beep_init(&g, RATE);
    finder_beep_set(&g, 5, false);
    finder_beep_next(&g, buf, 2048);
    for (uint32_t i = 0; i < 2048; i++) {
        assert(abs(buf[i]) <= FINDER_BEEP_PEAK);
    }
}

// 3000Hz、40ms 应有约 240 次符号翻转（每周期两次）。
// 频率由 Q32 相位累加器给出，不受整数除法误差影响（原 demo 会得到 3200Hz）。
static void test_tone_frequency_is_3000hz(void)
{
    finder_beep_init(&g, RATE);
    finder_beep_set(&g, 5, false);
    finder_beep_next(&g, buf, 640);
    int flips = count_sign_flips(buf, 640);
    assert(flips >= 230 && flips <= 250);
}

// 起手不能以满量程阶跃进入——那是可听的咔哒。
static void test_fade_in_starts_at_zero_and_rises(void)
{
    finder_beep_init(&g, RATE);
    finder_beep_set(&g, 5, false);
    finder_beep_next(&g, buf, 64);
    assert(buf[0] == 0);
    assert(abs(buf[FINDER_BEEP_FADE_MS * RATE / 1000 - 1]) > abs(buf[1]));
}

// 核心性质：输出只取决于样本计数，与调用方如何分块无关。
// 若没有这条，"每响一声写一次"的时序漂移就会回来。
static void test_chunking_does_not_change_waveform(void)
{
    finder_beep_gen_t a;
    finder_beep_gen_t b;
    finder_beep_init(&a, RATE);
    finder_beep_set(&a, 5, false);
    finder_beep_init(&b, RATE);
    finder_beep_set(&b, 5, false);

    assert(finder_beep_next(&a, buf, 1280) == 1280);

    uint32_t done = 0;
    while (done < 1280) {
        uint32_t n = (1280 - done < 97) ? (1280 - done) : 97;
        assert(finder_beep_next(&b, buf2 + done, n) == n);
        done += n;
    }
    assert(memcmp(buf, buf2, sizeof(int16_t) * 1280) == 0);
}

// 静音只关闭输出，节奏时钟继续走：取消静音后应立刻接上当下状态，
// 而不是从一个被冻结的位置重来。
// 对齐要求：被对比的两段必须落在【相同的周期位置】上，所以两边累计推进的
// 样本数必须相等——这正是本测试要证明的东西（静音期间时钟照走）。
static void test_mute_only_gates_output(void)
{
    finder_beep_gen_t a;
    finder_beep_gen_t b;
    finder_beep_init(&a, RATE);
    finder_beep_set(&a, 5, false);
    finder_beep_init(&b, RATE);
    finder_beep_set(&b, 5, false);

    // 两边同步走到位置 300
    finder_beep_next(&a, buf, 300);
    finder_beep_next(&b, buf2, 300);

    // a 提前走完与"静音段"等长的一段（位置 300 → 600）
    finder_beep_next(&a, buf, 300);

    // b 在同一段里被静音：输出全零，但时钟照走（位置 300 → 600）
    finder_beep_set(&b, 5, true);
    assert(finder_beep_next(&b, buf2, 300) == 300);
    assert(all_zero(buf2, 300));
    finder_beep_set(&b, 5, false);

    // 现在两者都在位置 600，接下来的 300 个样本必须逐字节一致
    finder_beep_next(&a, buf, 300);
    finder_beep_next(&b, buf2, 300);
    assert(memcmp(buf, buf2, sizeof(int16_t) * 300) == 0);
}

// 等级变化时节奏从头开始：用户靠得更近应当立刻听到回应。
static void test_level_change_restarts_cycle(void)
{
    finder_beep_init(&g, RATE);
    finder_beep_set(&g, 2, false); // 周期 1000ms，发声 40ms
    finder_beep_next(&g, buf, 1000);
    assert(all_zero(buf + 640, 360)); // 已进入本轮的静默段

    finder_beep_set(&g, 5, false); // 应当立刻重新起鸣
    finder_beep_next(&g, buf, 32);
    int nonzero = 0;
    for (uint32_t i = 0; i < 32; i++) {
        if (buf[i] != 0) {
            nonzero++;
        }
    }
    assert(nonzero > 0);
}

// 同等级重复 set 不得打断节奏（否则 UI 每帧刷新都会重启蜂鸣）。
static void test_setting_same_level_does_not_restart(void)
{
    finder_beep_gen_t a;
    finder_beep_gen_t b;
    finder_beep_init(&a, RATE);
    finder_beep_set(&a, 4, false);
    finder_beep_init(&b, RATE);
    finder_beep_set(&b, 4, false);

    finder_beep_next(&a, buf, 100);
    finder_beep_next(&b, buf2, 100);

    finder_beep_set(&b, 4, false); // 重复设置同一等级

    finder_beep_next(&a, buf, 100);
    finder_beep_next(&b, buf2, 100);
    assert(memcmp(buf, buf2, sizeof(int16_t) * 100) == 0);
}

int main(void)
{
    test_cadence_table();
    test_level_zero_is_silent();
    test_mute_is_silent();
    test_level5_cycle_shape();
    test_amplitude_stays_within_peak();
    test_tone_frequency_is_3000hz();
    test_fade_in_starts_at_zero_and_rises();
    test_chunking_does_not_change_waveform();
    test_mute_only_gates_output();
    test_level_change_restarts_cycle();
    test_setting_same_level_does_not_restart();

    printf("test_finder_beep: all assertions passed\n");
    return 0;
}
