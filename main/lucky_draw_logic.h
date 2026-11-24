// main/lucky_draw_logic.h —— 快乐选号纯逻辑:抽样、结果校验与页面状态机。
// 与 ESP-IDF/LVGL 完全解耦,host 测试直接编译本文件。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define LUCKY_NUM_MIN 1
#define LUCKY_NUM_MAX 80
#define LUCKY_NUM_RANGE (LUCKY_NUM_MAX - LUCKY_NUM_MIN + 1)
#define LUCKY_COUNT_MIN 1
#define LUCKY_COUNT_MAX 9

// 随机源回调:返回 32 位随机数(设备层接 esp_random,host 测试注入固定序列)。
typedef uint32_t (*lucky_rng_fn)(void);

// 把个数夹紧到 [LUCKY_COUNT_MIN, LUCKY_COUNT_MAX]。
int lucky_count_clamp(int count);

// 抽取 count 个 [1,80] 互不相同的号码,按升序写入 out。
// out 容量必须不小于 LUCKY_COUNT_MAX。rng 为 NULL 或 count 越界时返回 false。
bool lucky_draw_numbers(lucky_rng_fn rng, int count, uint8_t *out);

// 校验一组结果:count 合法、全部在范围内、升序且互不相同。
bool lucky_numbers_valid(const uint8_t *nums, int count);

// ---------------------------------------------------------------------------
// 页面状态机
// ---------------------------------------------------------------------------

typedef enum {
    LUCKY_PAGE_SETUP,   // 选择号码个数
    LUCKY_PAGE_RESULT,  // 展示最近一组结果
} lucky_page_t;

typedef enum {
    LUCKY_KEY_UP_CLICK,
    LUCKY_KEY_DOWN_CLICK,
    LUCKY_KEY_OK_CLICK,
    LUCKY_KEY_OK_LONG,
} lucky_input_t;

typedef struct {
    lucky_page_t page;
    int count;                       // SETUP 页当前选择的号码个数
    uint8_t nums[LUCKY_COUNT_MAX];   // 最近一组结果(升序)
    int nums_len;                    // 0 表示尚无结果
} lucky_state_t;

// 输入处理。返回 true 表示状态有变化;need_draw 置 true 时宿主应调用
// lucky_draw_numbers() 生成新结果再渲染。st 为 NULL 时返回 false。
bool lucky_handle_input(lucky_state_t *st, lucky_input_t in, bool *need_draw);
