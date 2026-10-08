// main/finder_ui.h —— Pocket Finder 的页面状态机（纯逻辑，可宿主测试）。
//
// 刻意不包含 bsp_button.h：输入用自定义枚举表达，由 ESP-IDF 侧把
// (bsp_btn_t, bsp_btn_ev_t) 翻译成 finder_input_t。这样本模块能在宿主机上直接测试，
// 也不会把按钮组件的实现细节漏进产品逻辑。
//
// 页面与按键定义见 PRD §6 / §8。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FINDER_EXIT_ARM_MS   3000 // 两步退出：第一步后确认层的有效期
#define FINDER_NOTICE_MS     1000 // 瞬时提示的显示时长

typedef enum {
    FINDER_INPUT_NONE = 0,
    FINDER_INPUT_UP_CLICK,
    FINDER_INPUT_DOWN_CLICK,
    FINDER_INPUT_OK_CLICK,
    FINDER_INPUT_OK_LONG,
    FINDER_INPUT_UP_LONG,
    FINDER_INPUT_DOWN_LONG,
} finder_input_t;

typedef enum {
    FINDER_PAGE_INTRO = 0, // 首次使用说明（每个固件版本一次）
    FINDER_PAGE_RADAR,     // 环形多设备雷达
    FINDER_PAGE_FOCUS,     // 单设备精细追踪
} finder_page_t;

typedef enum {
    FINDER_ACTION_NONE = 0,
    FINDER_ACTION_REDRAW,         // 界面需要重画
    FINDER_ACTION_LEAVE,          // 离开本应用，回到菜单
    FINDER_ACTION_NEXT_DEVICE,    // 聚焦页：切到下一个设备（调用方查表并回填选中项）
    FINDER_ACTION_INTRO_SEEN,     // 说明页已读，调用方持久化该标记
} finder_action_t;

typedef enum {
    FINDER_NOTICE_NONE = 0,
    FINDER_NOTICE_DEVICE_GONE, // 聚焦中的设备已从表中消失
    FINDER_NOTICE_ONLY_DEVICE, // 只有一个设备，"下一个"无处可去
    FINDER_NOTICE_NEXT,        // 刚切到下一个设备（显示 NEXT → 名称）
} finder_notice_t;

typedef struct {
    finder_page_t   page;
    int8_t          selected;     // 雷达页选中项；-1 表示无选中
    int8_t          focus;        // 聚焦页当前追踪的条目下标
    bool            muted;
    bool            show_diag;    // 长按 UP 显示诊断数值
    bool            exit_armed;   // 两步退出的第一步已完成
    int64_t         exit_armed_ms;
    finder_notice_t notice;
    int64_t         notice_ms;
} finder_ui_t;

typedef struct {
    finder_action_t action;
    int8_t          index;
} finder_ui_result_t;

// intro_seen 为 false 时从说明页开始，否则直接进入雷达页。
void finder_ui_init(finder_ui_t *ui, bool intro_seen);

// device_count 是本帧设备表中的条目数；调用方每帧传入真实值。
finder_ui_result_t finder_ui_handle(finder_ui_t *ui, finder_input_t input,
                                    int64_t now_ms, uint8_t device_count);

// 周期性调用：撤销超时的退出确认、清除过期的瞬时提示。
// 返回 true 表示界面需要重画。
bool finder_ui_tick(finder_ui_t *ui, int64_t now_ms);

// 调用方在设备表变化后同步选中项（例如老化导致条目消失、或执行了"下一个"）。
void finder_ui_select(finder_ui_t *ui, int8_t index);
void finder_ui_set_page(finder_ui_t *ui, finder_page_t page);
void finder_ui_set_notice(finder_ui_t *ui, finder_notice_t notice, int64_t now_ms);
