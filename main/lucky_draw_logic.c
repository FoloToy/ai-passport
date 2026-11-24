// main/lucky_draw_logic.c —— 快乐选号纯逻辑实现。
// 抽样用部分 Fisher-Yates:在 1..80 的值表上做 count 轮交换,取前 count 个,
// 再插入排序为升序。80 个 uint8_t 放栈上,无动态分配。
#include "lucky_draw_logic.h"

#include <string.h>

int lucky_count_clamp(int count) {
    if (count < LUCKY_COUNT_MIN) return LUCKY_COUNT_MIN;
    if (count > LUCKY_COUNT_MAX) return LUCKY_COUNT_MAX;
    return count;
}

bool lucky_numbers_valid(const uint8_t *nums, int count) {
    if (count < LUCKY_COUNT_MIN || count > LUCKY_COUNT_MAX) return false;
    if (nums == NULL) return false;
    for (int i = 0; i < count; i++) {
        if (nums[i] < LUCKY_NUM_MIN || nums[i] > LUCKY_NUM_MAX) return false;
        // 升序蕴含互不相同:允许相等或倒序都判无效。
        if (i > 0 && nums[i] <= nums[i - 1]) return false;
    }
    return true;
}

bool lucky_draw_numbers(lucky_rng_fn rng, int count, uint8_t *out) {
    if (rng == NULL || out == NULL) return false;
    if (count < LUCKY_COUNT_MIN || count > LUCKY_COUNT_MAX) return false;

    uint8_t pool[LUCKY_NUM_RANGE];
    for (int i = 0; i < LUCKY_NUM_RANGE; i++) {
        pool[i] = (uint8_t)(LUCKY_NUM_MIN + i);
    }

    // 未洗牌的尾部 [i, range) 里随机取一个位置,换到位置 i。
    for (int i = 0; i < count; i++) {
        uint32_t r = rng();
        // 无偏取模:Lemons 方法,拒绝会落进最后一个不完整区间的样本。
        uint32_t remain = (uint32_t)(LUCKY_NUM_RANGE - i);
        uint32_t limit = UINT32_MAX - UINT32_MAX % remain;
        while (r >= limit) {
            r = rng();
        }
        int j = (int)(i + r % remain);
        uint8_t tmp = pool[i];
        pool[i] = pool[j];
        pool[j] = tmp;
    }

    // 前 count 个已互不相同,插入排序为升序。
    for (int i = 1; i < count; i++) {
        uint8_t key = pool[i];
        int j = i - 1;
        while (j >= 0 && pool[j] > key) {
            pool[j + 1] = pool[j];
            j--;
        }
        pool[j + 1] = key;
    }
    memcpy(out, pool, (size_t)count);
    return true;
}

bool lucky_handle_input(lucky_state_t *st, lucky_input_t in, bool *need_draw) {
    if (st == NULL) return false;
    if (need_draw) *need_draw = false;

    switch (st->page) {
        case LUCKY_PAGE_SETUP:
            switch (in) {
                case LUCKY_KEY_UP_CLICK:
                    // 两端夹紧而不是绕圈。
                    if (st->count < LUCKY_COUNT_MAX) {
                        st->count++;
                        return true;
                    }
                    return false;
                case LUCKY_KEY_DOWN_CLICK:
                    if (st->count > LUCKY_COUNT_MIN) {
                        st->count--;
                        return true;
                    }
                    return false;
                case LUCKY_KEY_OK_CLICK:
                    st->page = LUCKY_PAGE_RESULT;
                    if (need_draw) *need_draw = true;
                    return true;
                case LUCKY_KEY_OK_LONG:
                    return false;
                default:
                    return false;
            }
        case LUCKY_PAGE_RESULT:
            switch (in) {
                case LUCKY_KEY_OK_CLICK:
                    if (need_draw) *need_draw = true;
                    return true;
                case LUCKY_KEY_OK_LONG:
                    st->page = LUCKY_PAGE_SETUP;
                    return true;
                default:
                    return false;
            }
        default:
            return false;
    }
}
