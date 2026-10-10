// tests/test_finder_idle.c —— finder_idle 的宿主测试（不需要 ESP-IDF）。
#include <assert.h>
#include <stdio.h>

#include "finder_idle.h"

#define DIM FINDER_IDLE_DIM_MS
#define OFF FINDER_IDLE_OFF_MS

static void test_starts_awake(void)
{
    finder_idle_t idle;
    finder_idle_init(&idle, 1000);
    assert(finder_idle_state_at(&idle, 1000) == FINDER_IDLE_AWAKE);
}

static void test_thresholds(void)
{
    finder_idle_t idle;
    finder_idle_init(&idle, 0);

    // 边界：差 1ms 仍在上一档，正好到达即进入下一档。
    assert(finder_idle_state_at(&idle, DIM - 1) == FINDER_IDLE_AWAKE);
    assert(finder_idle_state_at(&idle, DIM) == FINDER_IDLE_DIM);
    assert(finder_idle_state_at(&idle, OFF - 1) == FINDER_IDLE_DIM);
    assert(finder_idle_state_at(&idle, OFF) == FINDER_IDLE_OFF);
    // 一直没人管也不能越过息屏跑到别的档位
    assert(finder_idle_state_at(&idle, OFF * 4) == FINDER_IDLE_OFF);
}

static void test_backlight_mapping(void)
{
    assert(finder_idle_backlight(FINDER_IDLE_AWAKE) == 100);
    assert(finder_idle_backlight(FINDER_IDLE_DIM) == 50);
    assert(finder_idle_backlight(FINDER_IDLE_OFF) == 0);
    // "减半"必须真的是全亮的一半：改常量时这条会挡住手滑。
    assert(finder_idle_backlight(FINDER_IDLE_DIM) * 2 ==
           finder_idle_backlight(FINDER_IDLE_AWAKE));
}

static void test_dim_does_not_swallow_keys(void)
{
    finder_idle_t idle;
    finder_idle_init(&idle, 0);
    // 半亮时屏幕看得见，按键必须照常生效
    assert(finder_idle_note_key(&idle, OFF - 1) == false);
}

static void test_key_resets_the_clock(void)
{
    finder_idle_t idle;
    finder_idle_init(&idle, 0);

    assert(finder_idle_note_key(&idle, DIM) == false);
    // 刚按过之后的一整个窗口内不该降亮
    assert(finder_idle_state_at(&idle, DIM + DIM - 1) == FINDER_IDLE_AWAKE);
    assert(finder_idle_state_at(&idle, DIM + DIM) == FINDER_IDLE_DIM);

    assert(finder_idle_note_key(&idle, DIM + DIM) == false);
    assert(finder_idle_state_at(&idle, DIM + DIM + DIM) == FINDER_IDLE_DIM);
    assert(finder_idle_state_at(&idle, DIM + DIM + OFF) == FINDER_IDLE_OFF);
}

static void test_off_state_swallows_the_wake_press(void)
{
    finder_idle_t idle;
    finder_idle_init(&idle, 0);

    assert(finder_idle_note_key(&idle, OFF) == true);  // PRESS：只唤醒
    assert(finder_idle_state_at(&idle, OFF) == FINDER_IDLE_AWAKE);
    // 唤醒时重新计时，不会点亮后立刻又息屏
    assert(finder_idle_state_at(&idle, OFF + DIM - 1) == FINDER_IDLE_AWAKE);
}

static void test_one_press_is_swallowed_as_a_whole(void)
{
    // 真实事件顺序是 PRESS → RELEASE → CLICK（iot_button），三者属于同一次按压。
    // 只吞 PRESS 会让紧随其后的 CLICK 仍然触发动作。
    finder_idle_t idle;
    finder_idle_init(&idle, 0);

    assert(finder_idle_note_key(&idle, OFF) == true);          // PRESS
    assert(finder_idle_note_key(&idle, OFF + 120) == true);    // RELEASE
    assert(finder_idle_note_key(&idle, OFF + 180) == true);    // CLICK
}

static void test_long_press_on_wake_is_swallowed(void)
{
    // 按住不放的用户只应点亮屏幕：500ms 的长按事件也必须落在宽限期内被吞掉，
    // 否则会在看不见屏幕时切换静音。
    finder_idle_t idle;
    finder_idle_init(&idle, 0);

    assert(finder_idle_note_key(&idle, OFF) == true);          // PRESS
    assert(finder_idle_note_key(&idle, OFF + 500) == true);    // LONG
    assert(finder_idle_note_key(&idle, OFF + 520) == true);    // RELEASE
}

static void test_key_after_the_wake_grace_acts(void)
{
    finder_idle_t idle;
    finder_idle_init(&idle, 0);

    assert(finder_idle_note_key(&idle, OFF) == true);
    // 宽限期边界：正好到期即恢复
    assert(finder_idle_note_key(&idle, OFF + FINDER_IDLE_WAKE_GRACE_MS - 1) == true);
    assert(finder_idle_note_key(&idle, OFF + DIM) == false);
}

static void test_second_distinct_press_after_wake_acts(void)
{
    finder_idle_t idle;
    finder_idle_init(&idle, 0);

    assert(finder_idle_note_key(&idle, OFF) == true);        // 唤醒
    assert(finder_idle_note_key(&idle, OFF + 1000) == false); // 用户看清后再按
}

static void test_wraparound_counter(void)
{
    // xTaskGetTickCount() 是 32 位毫秒计数，约 49 天回绕。
    // 无符号减法必须把"刚活动过"算成很短，而不是闲置了很久。
    finder_idle_t idle;
    finder_idle_init(&idle, 0xFFFFFF00u);

    assert(finder_idle_state_at(&idle, 0x00000100u) == FINDER_IDLE_AWAKE);  // 间隔 512ms
    assert(finder_idle_state_at(&idle, 0xFFFFFF00u + DIM) == FINDER_IDLE_DIM);
    assert(finder_idle_state_at(&idle, 0xFFFFFF00u + OFF) == FINDER_IDLE_OFF);
}

static void test_wake_grace_crosses_counter_wraparound(void)
{
    // 唤醒发生在 32 位计数回绕前 16ms，紧随其后的 RELEASE/CLICK 落在回绕之后。
    // 宽限期判定必须同样跨过回绕，否则用户在息屏唤醒的那一刻会误触动作。
    finder_idle_t idle;
    const uint32_t wake = 0xFFFFFFF0u;      // 回绕前 16ms

    finder_idle_init(&idle, wake - OFF);    // 已闲置到息屏
    assert(finder_idle_state_at(&idle, wake) == FINDER_IDLE_OFF);

    assert(finder_idle_note_key(&idle, wake) == true);          // PRESS：唤醒
    assert(finder_idle_note_key(&idle, 0x00000050u) == true);   // 回绕后 80ms 的 CLICK
    assert(finder_idle_note_key(&idle, 0x000002B8u) == false);  // 回绕后 712ms
}

int main(void)
{
    test_starts_awake();
    test_thresholds();
    test_backlight_mapping();
    test_dim_does_not_swallow_keys();
    test_key_resets_the_clock();
    test_off_state_swallows_the_wake_press();
    test_one_press_is_swallowed_as_a_whole();
    test_long_press_on_wake_is_swallowed();
    test_key_after_the_wake_grace_acts();
    test_second_distinct_press_after_wake_acts();
    test_wraparound_counter();
    test_wake_grace_crosses_counter_wraparound();

    printf("test_finder_idle: all assertions passed\n");
    return 0;
}
