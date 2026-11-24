// tests/test_lucky_draw_logic.c —— 快乐选号纯逻辑 host 测试:抽样、校验与状态机。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "lucky_draw_logic.h"

// 确定性 rng:LCG,测试里可复现;同时提供注入序列模式。
static uint32_t g_lcg_state;
static uint32_t rng_lcg(void) {
    g_lcg_state = g_lcg_state * 1664525u + 1013904223u;
    return g_lcg_state;
}

static uint32_t g_seq[16];
static size_t g_seq_len, g_seq_idx;
static uint32_t rng_seq(void) {
    assert(g_seq_idx < g_seq_len);
    return g_seq[g_seq_idx++];
}

int main(void) {
    // clamp:越界夹紧,合法值原样。
    assert(lucky_count_clamp(-3) == LUCKY_COUNT_MIN);
    assert(lucky_count_clamp(0) == LUCKY_COUNT_MIN);
    assert(lucky_count_clamp(1) == 1);
    assert(lucky_count_clamp(5) == 5);
    assert(lucky_count_clamp(9) == 9);
    assert(lucky_count_clamp(10) == LUCKY_COUNT_MAX);
    assert(lucky_count_clamp(99) == LUCKY_COUNT_MAX);

    // draw:参数非法时拒绝。
    uint8_t nums[LUCKY_COUNT_MAX];
    assert(!lucky_draw_numbers(NULL, 5, nums));
    g_lcg_state = 42u;
    assert(!lucky_draw_numbers(rng_lcg, 0, nums));
    assert(!lucky_draw_numbers(rng_lcg, 10, nums));
    assert(!lucky_draw_numbers(rng_lcg, 5, NULL));

    // draw:确定性——同一 seed 两次结果一致且均通过校验。
    g_lcg_state = 7u;
    uint8_t a[LUCKY_COUNT_MAX], b[LUCKY_COUNT_MAX];
    assert(lucky_draw_numbers(rng_lcg, 6, a));
    g_lcg_state = 7u;
    assert(lucky_draw_numbers(rng_lcg, 6, b));
    assert(memcmp(a, b, 6) == 0);
    assert(lucky_numbers_valid(a, 6));

    // draw:全部合法个数下,结果都在范围内、无重复、升序;多种 seed 反复验证。
    for (uint32_t seed = 0; seed < 50u; seed++) {
        for (int count = LUCKY_COUNT_MIN; count <= LUCKY_COUNT_MAX; count++) {
            g_lcg_state = seed * 7919u + (uint32_t)count;
            assert(lucky_draw_numbers(rng_lcg, count, nums));
            assert(lucky_numbers_valid(nums, count));
            assert(nums[0] >= LUCKY_NUM_MIN && nums[count - 1] <= LUCKY_NUM_MAX);
        }
    }

    // draw:count=9 时首号不小于 1,末号不大于 80。
    g_lcg_state = 123456789u;
    assert(lucky_draw_numbers(rng_lcg, 9, nums));
    assert(nums[0] >= LUCKY_NUM_MIN && nums[8] <= LUCKY_NUM_MAX);

    // 无偏取模拒绝路径:构造恰好命中拒绝区间(尾段高值)的序列,应要求补采样。
    // UINT32_MAX = 4294967295;remain=80 时 limit = UINT32_MAX - 79 = 4294967216,
    // 首个样本落在 [4294967216, 4294967295] 会被拒绝,必须再取一个。
    g_seq_len = 0;
    g_seq_idx = 0;
    g_seq[g_seq_len++] = 4294967250u;  // 落在拒绝区间
    g_seq[g_seq_len++] = 1000u;        // 正常样本
    assert(lucky_draw_numbers(rng_seq, 1, nums));
    assert(nums[0] >= LUCKY_NUM_MIN && nums[0] <= LUCKY_NUM_MAX);

    // 拒绝后仍非法:两次都在拒绝区间时循环继续拒绝(构造 79 次拒绝后的场景
    // 不现实,这里验证连续拒绝不崩溃——用 reject+reject+valid,remain=80 会
    // 连续拒绝两次)。
    g_seq_idx = 0;
    g_seq_len = 0;
    g_seq[g_seq_len++] = 4294967216u;  // 恰好等于 limit,仍拒绝
    g_seq[g_seq_len++] = 4294967295u;  // 上界,仍拒绝
    g_seq[g_seq_len++] = 3u;
    assert(lucky_draw_numbers(rng_seq, 1, nums));

    // seq 耗尽时再取会触发 assert 失败,补一个保底样本确认算法不额外取随机数。
    g_seq_idx = 0;
    g_seq_len = 0;
    g_seq[g_seq_len++] = 0u;           // remain=80, j=0
    assert(lucky_draw_numbers(rng_seq, 1, nums));
    assert(nums[0] == 1);              // 选中的是 pool[0]=1

    // 校验器:合法/越界/重复/倒序/空指针。
    const uint8_t good[3] = {2, 41, 80};
    assert(lucky_numbers_valid(good, 3));
    const uint8_t dup[2] = {5, 5};
    assert(!lucky_numbers_valid(dup, 2));
    const uint8_t desc[2] = {9, 4};
    assert(!lucky_numbers_valid(desc, 2));
    const uint8_t out_of_range[1] = {81};
    assert(!lucky_numbers_valid(out_of_range, 1));
    const uint8_t zero[1] = {0};
    assert(!lucky_numbers_valid(zero, 1));
    assert(!lucky_numbers_valid(good, 0));
    assert(!lucky_numbers_valid(good, LUCKY_COUNT_MAX + 1));
    assert(!lucky_numbers_valid(NULL, 3));

    // 状态机:SETUP 页 UP/DOWN 调个数,两端夹紧。
    lucky_state_t st = {0};
    st.page = LUCKY_PAGE_SETUP;
    st.count = 6;
    bool need_draw = true;  // 非法输入不得误置 need_draw
    assert(!lucky_handle_input(&st, LUCKY_KEY_OK_LONG, &need_draw));
    assert(need_draw == false);
    assert(lucky_handle_input(&st, LUCKY_KEY_DOWN_CLICK, &need_draw));
    assert(st.count == 5 && !need_draw);
    assert(lucky_handle_input(&st, LUCKY_KEY_UP_CLICK, &need_draw));
    assert(st.count == 6 && !need_draw);
    st.count = LUCKY_COUNT_MAX;
    assert(!lucky_handle_input(&st, LUCKY_KEY_UP_CLICK, &need_draw));
    assert(st.count == LUCKY_COUNT_MAX);
    st.count = LUCKY_COUNT_MIN;
    assert(!lucky_handle_input(&st, LUCKY_KEY_DOWN_CLICK, &need_draw));
    assert(st.count == LUCKY_COUNT_MIN);

    // 状态机:SETUP OK 进结果页并要求抽取。
    st.count = 3;
    assert(lucky_handle_input(&st, LUCKY_KEY_OK_CLICK, &need_draw));
    assert(st.page == LUCKY_PAGE_RESULT && need_draw);

    // 状态机:结果页 OK 再抽一组,长按回设置页。
    assert(lucky_handle_input(&st, LUCKY_KEY_OK_CLICK, &need_draw));
    assert(st.page == LUCKY_PAGE_RESULT && need_draw);
    assert(!lucky_handle_input(&st, LUCKY_KEY_UP_CLICK, &need_draw));
    assert(!need_draw);
    assert(lucky_handle_input(&st, LUCKY_KEY_OK_LONG, &need_draw));
    assert(st.page == LUCKY_PAGE_SETUP && !need_draw);

    // 空状态指针。
    assert(!lucky_handle_input(NULL, LUCKY_KEY_OK_CLICK, &need_draw));

    printf("test_lucky_draw_logic: PASS\n");
    return 0;
}
