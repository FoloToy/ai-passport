// tests/test_finder_ui.c —— finder_ui 状态机的宿主测试（不需要 ESP-IDF）。
#include <assert.h>
#include <stdio.h>

#include "finder_ui.h"

#define T0 1000000

static finder_ui_t g;

static finder_ui_result_t feed(finder_input_t input, int64_t now_ms, uint8_t count)
{
    return finder_ui_handle(&g, input, now_ms, count);
}

static void test_intro_is_mandatory_and_dismissed_by_ok(void)
{
    finder_ui_init(&g, false);
    assert(g.page == FINDER_PAGE_INTRO);

    // 说明页只能被确认键翻过：这是每个固件版本一次的必读页。
    assert(feed(FINDER_INPUT_UP_CLICK, T0, 3).action == FINDER_ACTION_NONE);
    assert(feed(FINDER_INPUT_DOWN_LONG, T0, 3).action == FINDER_ACTION_NONE);
    assert(g.page == FINDER_PAGE_INTRO);

    assert(feed(FINDER_INPUT_OK_CLICK, T0, 3).action == FINDER_ACTION_INTRO_SEEN);
    assert(g.page == FINDER_PAGE_RADAR);
}

static void test_radar_entry_when_intro_already_seen(void)
{
    finder_ui_init(&g, true);
    assert(g.page == FINDER_PAGE_RADAR);
    assert(g.selected == -1);
}

static void test_radar_selection_cycles_and_first_press_picks_first(void)
{
    finder_ui_init(&g, true);
    assert(feed(FINDER_INPUT_UP_CLICK, T0, 3).action == FINDER_ACTION_REDRAW);
    assert(g.selected == 0); // 首次按键落到第一项

    feed(FINDER_INPUT_DOWN_CLICK, T0, 3);
    assert(g.selected == 1);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 3);
    assert(g.selected == 2);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 3);
    assert(g.selected == 0); // 回绕
    feed(FINDER_INPUT_UP_CLICK, T0, 3);
    assert(g.selected == 2); // 反向回绕
}

static void test_radar_selection_ignored_without_devices(void)
{
    finder_ui_init(&g, true);
    assert(feed(FINDER_INPUT_DOWN_CLICK, T0, 0).action == FINDER_ACTION_NONE);
    assert(g.selected == -1);
}

static void test_radar_ok_enters_focus_only_with_valid_selection(void)
{
    finder_ui_init(&g, true);

    // 没有任何设备：不得进入空聚焦页
    assert(feed(FINDER_INPUT_OK_CLICK, T0, 0).action == FINDER_ACTION_NONE);
    assert(g.page == FINDER_PAGE_RADAR);

    // 有设备但没有选中项
    assert(feed(FINDER_INPUT_OK_CLICK, T0, 2).action == FINDER_ACTION_NONE);

    feed(FINDER_INPUT_DOWN_CLICK, T0, 2);
    assert(feed(FINDER_INPUT_OK_CLICK, T0, 2).action == FINDER_ACTION_REDRAW);
    assert(g.page == FINDER_PAGE_FOCUS);
    assert(g.focus == 0);
}

// 选中项被老化移除后，OK 不得进入一个不存在的条目。
static void test_radar_ok_rejected_when_selection_aged_out(void)
{
    finder_ui_init(&g, true);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 3);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 3);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 3);
    assert(g.selected == 2);

    assert(feed(FINDER_INPUT_OK_CLICK, T0, 2).action == FINDER_ACTION_NONE); // 只剩 2 项
    assert(g.page == FINDER_PAGE_RADAR);
}

static void test_mute_toggles_on_long_ok_in_both_pages(void)
{
    finder_ui_init(&g, true);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 1);
    feed(FINDER_INPUT_OK_CLICK, T0, 1);
    assert(g.page == FINDER_PAGE_FOCUS);

    assert(feed(FINDER_INPUT_OK_LONG, T0, 1).action == FINDER_ACTION_REDRAW);
    assert(g.muted);
    feed(FINDER_INPUT_OK_LONG, T0, 1);
    assert(!g.muted);
}

static void test_focus_ok_requests_next_device(void)
{
    finder_ui_init(&g, true);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 3);
    feed(FINDER_INPUT_OK_CLICK, T0, 3);
    assert(g.page == FINDER_PAGE_FOCUS);

    finder_ui_result_t r = feed(FINDER_INPUT_OK_CLICK, T0, 3);
    assert(r.action == FINDER_ACTION_NEXT_DEVICE);
    assert(r.index == g.focus);
    assert(g.page == FINDER_PAGE_FOCUS); // 页面切换由调用方完成
}

static void test_focus_diagnostics_toggle(void)
{
    finder_ui_init(&g, true);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 1);
    feed(FINDER_INPUT_OK_CLICK, T0, 1);

    assert(!g.show_diag);
    assert(feed(FINDER_INPUT_UP_LONG, T0, 1).action == FINDER_ACTION_REDRAW);
    assert(g.show_diag);
    feed(FINDER_INPUT_UP_LONG, T0, 1);
    assert(!g.show_diag);

    // 聚焦页不消费上/下短按
    assert(feed(FINDER_INPUT_UP_CLICK, T0, 1).action == FINDER_ACTION_NONE);
}

// 两步退出：长按 DOWN 是破坏性动作，第一次只武装并显示确认层。
static void test_two_step_exit_from_radar(void)
{
    finder_ui_init(&g, true);

    finder_ui_result_t first = feed(FINDER_INPUT_DOWN_LONG, T0, 2);
    assert(first.action == FINDER_ACTION_REDRAW); // 显示确认层，不是退出
    assert(g.exit_armed);

    finder_ui_result_t second = feed(FINDER_INPUT_DOWN_LONG, T0 + 500, 2);
    assert(second.action == FINDER_ACTION_LEAVE);
    assert(!g.exit_armed);
}

static void test_two_step_exit_arm_expires(void)
{
    finder_ui_init(&g, true);
    feed(FINDER_INPUT_DOWN_LONG, T0, 1);

    // 超时后再次长按只应重新武装，不得退出
    finder_ui_result_t late = feed(FINDER_INPUT_DOWN_LONG, T0 + FINDER_EXIT_ARM_MS + 1, 1);
    assert(late.action == FINDER_ACTION_REDRAW);
    assert(g.exit_armed);
}

static void test_exit_arm_cancelled_by_other_input(void)
{
    finder_ui_init(&g, true);
    feed(FINDER_INPUT_DOWN_LONG, T0, 2);
    assert(g.exit_armed);

    feed(FINDER_INPUT_UP_CLICK, T0 + 100, 2); // 任何其它按键都取消待确认
    assert(!g.exit_armed);

    finder_ui_result_t again = feed(FINDER_INPUT_DOWN_LONG, T0 + 200, 2);
    assert(again.action == FINDER_ACTION_REDRAW); // 重新武装，不是退出
}

static void test_focus_long_down_returns_to_radar_not_menu(void)
{
    finder_ui_init(&g, true);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 2);
    feed(FINDER_INPUT_OK_CLICK, T0, 2);
    assert(g.page == FINDER_PAGE_FOCUS);

    feed(FINDER_INPUT_DOWN_LONG, T0 + 10, 2);
    finder_ui_result_t r = feed(FINDER_INPUT_DOWN_LONG, T0 + 400, 2);
    assert(r.action == FINDER_ACTION_REDRAW);
    assert(g.page == FINDER_PAGE_RADAR); // 退回雷达页，而不是离开应用
}

static void test_tick_clears_expired_arm(void)
{
    finder_ui_init(&g, true);
    feed(FINDER_INPUT_DOWN_LONG, T0, 1);
    assert(!finder_ui_tick(&g, T0 + FINDER_EXIT_ARM_MS));     // 边界内仍有效
    assert(g.exit_armed);
    assert(finder_ui_tick(&g, T0 + FINDER_EXIT_ARM_MS + 1));  // 超时撤销并请求重画
    assert(!g.exit_armed);
    assert(!finder_ui_tick(&g, T0 + FINDER_EXIT_ARM_MS + 2)); // 已撤销则不再请求
}

static void test_notice_lifecycle(void)
{
    finder_ui_init(&g, true);
    finder_ui_set_notice(&g, FINDER_NOTICE_NEXT, T0);
    assert(g.notice == FINDER_NOTICE_NEXT);

    assert(!finder_ui_tick(&g, T0 + FINDER_NOTICE_MS - 1));
    assert(g.notice == FINDER_NOTICE_NEXT);
    assert(finder_ui_tick(&g, T0 + FINDER_NOTICE_MS));
    assert(g.notice == FINDER_NOTICE_NONE);
}

static void test_select_syncs_focus_page(void)
{
    finder_ui_init(&g, true);
    feed(FINDER_INPUT_DOWN_CLICK, T0, 3);
    feed(FINDER_INPUT_OK_CLICK, T0, 3);

    finder_ui_select(&g, 2);
    assert(g.selected == 2);
    assert(g.focus == 2); // 聚焦页上同步的是追踪目标
}

int main(void)
{
    test_intro_is_mandatory_and_dismissed_by_ok();
    test_radar_entry_when_intro_already_seen();
    test_radar_selection_cycles_and_first_press_picks_first();
    test_radar_selection_ignored_without_devices();
    test_radar_ok_enters_focus_only_with_valid_selection();
    test_radar_ok_rejected_when_selection_aged_out();
    test_mute_toggles_on_long_ok_in_both_pages();
    test_focus_ok_requests_next_device();
    test_focus_diagnostics_toggle();
    test_two_step_exit_from_radar();
    test_two_step_exit_arm_expires();
    test_exit_arm_cancelled_by_other_input();
    test_focus_long_down_returns_to_radar_not_menu();
    test_tick_clears_expired_arm();
    test_notice_lifecycle();
    test_select_syncs_focus_page();

    printf("test_finder_ui: all assertions passed\n");
    return 0;
}
