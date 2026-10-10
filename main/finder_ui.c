// main/finder_ui.c —— 见 finder_ui.h。纯 C，无 ESP-IDF / LVGL 依赖。
#include "finder_ui.h"

static finder_ui_result_t result(finder_action_t action, int8_t index)
{
    finder_ui_result_t r;
    r.action = action;
    r.index = index;
    return r;
}

void finder_ui_init(finder_ui_t *ui, bool intro_seen)
{
    if (!ui) {
        return;
    }
    ui->page = intro_seen ? FINDER_PAGE_RADAR : FINDER_PAGE_INTRO;
    ui->selected = -1;
    ui->focus = -1;
    ui->muted = false;
    ui->show_diag = false;
    ui->exit_armed = false;
    ui->exit_armed_ms = 0;
    ui->notice = FINDER_NOTICE_NONE;
    ui->notice_ms = 0;
}

void finder_ui_select(finder_ui_t *ui, int8_t index)
{
    if (!ui) {
        return;
    }
    ui->selected = index;
    if (ui->page == FINDER_PAGE_FOCUS) {
        ui->focus = index;
    }
}

void finder_ui_set_page(finder_ui_t *ui, finder_page_t page)
{
    if (ui) {
        ui->page = page;
    }
}

void finder_ui_set_notice(finder_ui_t *ui, finder_notice_t notice, int64_t now_ms)
{
    if (!ui) {
        return;
    }
    ui->notice = notice;
    ui->notice_ms = now_ms;
}

static finder_ui_result_t handle_radar(finder_ui_t *ui, finder_input_t input, uint8_t count)
{
    switch (input) {
    case FINDER_INPUT_UP_CLICK:
        if (count == 0) {
            return result(FINDER_ACTION_NONE, ui->selected);
        }
        // 首次按键（selected 仍为 -1）落到第一项。
        ui->selected = (ui->selected < 0)
                     ? 0
                     : (int8_t)((ui->selected + count - 1) % count);
        return result(FINDER_ACTION_REDRAW, ui->selected);

    case FINDER_INPUT_DOWN_CLICK:
        if (count == 0) {
            return result(FINDER_ACTION_NONE, ui->selected);
        }
        ui->selected = (ui->selected < 0)
                     ? 0
                     : (int8_t)((ui->selected + 1) % count);
        return result(FINDER_ACTION_REDRAW, ui->selected);

    case FINDER_INPUT_OK_CLICK:
        // 只有确实存在可进入的条目才切页，避免进入一个空聚焦页。
        if (count > 0 && ui->selected >= 0 && ui->selected < (int8_t)count) {
            ui->focus = ui->selected;
            ui->page = FINDER_PAGE_FOCUS;
            return result(FINDER_ACTION_REDRAW, ui->focus);
        }
        return result(FINDER_ACTION_NONE, ui->selected);

    case FINDER_INPUT_OK_LONG:
        ui->muted = !ui->muted;
        return result(FINDER_ACTION_REDRAW, ui->selected);

    default:
        return result(FINDER_ACTION_NONE, ui->selected);
    }
}

static finder_ui_result_t handle_focus(finder_ui_t *ui, finder_input_t input)
{
    switch (input) {
    case FINDER_INPUT_OK_CLICK:
        // 切下一个由调用方查表完成；本模块只表达意图。
        return result(FINDER_ACTION_NEXT_DEVICE, ui->focus);

    case FINDER_INPUT_OK_LONG:
        ui->muted = !ui->muted;
        return result(FINDER_ACTION_REDRAW, ui->focus);

    case FINDER_INPUT_UP_LONG:
        ui->show_diag = !ui->show_diag;
        return result(FINDER_ACTION_REDRAW, ui->focus);

    default:
        return result(FINDER_ACTION_NONE, ui->focus);
    }
}

finder_ui_result_t finder_ui_handle(finder_ui_t *ui, finder_input_t input,
                                    int64_t now_ms, uint8_t device_count)
{
    if (!ui) {
        return result(FINDER_ACTION_NONE, -1);
    }
    if (input == FINDER_INPUT_NONE) {
        return result(FINDER_ACTION_NONE, ui->selected);
    }

    // 说明页只认确认键：这是每个固件版本一次的必读页，不能被随手按掉。
    if (ui->page == FINDER_PAGE_INTRO) {
        if (input == FINDER_INPUT_OK_CLICK) {
            ui->page = FINDER_PAGE_RADAR;
            return result(FINDER_ACTION_INTRO_SEEN, ui->selected);
        }
        return result(FINDER_ACTION_NONE, ui->selected);
    }

    // 两步退出：任何其它按键都取消待确认状态（不吞掉本次输入）。
    if (input != FINDER_INPUT_DOWN_LONG && ui->exit_armed) {
        ui->exit_armed = false;
    }

    if (input == FINDER_INPUT_DOWN_LONG) {
        if (ui->exit_armed && now_ms - ui->exit_armed_ms <= FINDER_EXIT_ARM_MS) {
            ui->exit_armed = false;
            if (ui->page == FINDER_PAGE_FOCUS) {
                ui->page = FINDER_PAGE_RADAR; // 聚焦页：退到雷达页
                return result(FINDER_ACTION_REDRAW, ui->selected);
            }
            return result(FINDER_ACTION_LEAVE, ui->selected);
        }
        // 第一步：武装并显示确认层。不直接退出——长按走的是 500ms ADC 阶梯，
        // 误触率不低，而退出是破坏性动作。
        ui->exit_armed = true;
        ui->exit_armed_ms = now_ms;
        return result(FINDER_ACTION_REDRAW, ui->selected);
    }

    if (ui->page == FINDER_PAGE_FOCUS) {
        return handle_focus(ui, input);
    }
    return handle_radar(ui, input, device_count);
}

bool finder_ui_tick(finder_ui_t *ui, int64_t now_ms)
{
    if (!ui) {
        return false;
    }
    bool redraw = false;

    if (ui->exit_armed && now_ms - ui->exit_armed_ms > FINDER_EXIT_ARM_MS) {
        ui->exit_armed = false;
        redraw = true;
    }
    if (ui->notice != FINDER_NOTICE_NONE && now_ms - ui->notice_ms >= FINDER_NOTICE_MS) {
        ui->notice = FINDER_NOTICE_NONE;
        redraw = true;
    }
    return redraw;
}
