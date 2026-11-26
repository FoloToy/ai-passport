// main/treaai_ui.c — TreaAIPassport 重设计 UI。
//
// 设计说明:
//   - 不使用 baseline 的 ui_pixel_* 演示壳, 全部自建 LVGL 控件
//   - 三按钮导航语义:
//       首页: OK 短按进入选中卡片 / UP-DOWN 移动 / OK 长按进入对话
//       对话: OK 长按录音 / OK 短按发送 / OK 双击回首页
//       快捷指令: UP-DOWN 选 / OK 短按发送 / OK 双击回首页
//       设置: UP-DOWN 选 / OK 短按改 / OK 长按保存 / OK 双击回首页
//   - 右上角始终显示电量条, 内部每秒刷新
//   - AI 回复在 Chat 屏滚动列表显示

#include "treaai_ui.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "treaai_ui";

// ---------------------------------------------------------------------------
// 全局颜色 / 样式常量 (240x320 portrait)
// ---------------------------------------------------------------------------
#define BG_DARK     0x0B1020
#define BG_CARD     0x1A2038
#define ACCENT      0x34D399  // 青绿
#define ACCENT_DIM  0x1B5E4A
#define INK         0xE2E8F0
#define INK_DIM     0x8892A8
#define RED         0xF87171
#define BATTERY_BG  0x2A3048

// ---------------------------------------------------------------------------
// 状态
// ---------------------------------------------------------------------------

typedef struct {
    lv_obj_t *scr;

    // Home
    lv_obj_t *home_logo;
    lv_obj_t *home_subtitle;
    lv_obj_t *home_cards[4];    // Chat / QuickCmd / Settings / 暂留
    int       home_selected;    // 0..3
    lv_obj_t *home_status_label;

    // Chat
    lv_obj_t *chat_history;     // list widget
    lv_obj_t *chat_status_line; // "AI 思考中..." / "录音中..."
    int       chat_msg_count;

    // QuickCmd
    lv_obj_t *qc_cards[6];
    int       qc_selected;

    // Settings
    lv_obj_t *sett_cards[3];
    int       sett_selected;

    // 顶栏电量
    lv_obj_t *battery_bar;
    lv_obj_t *battery_fill;
    lv_obj_t *battery_pct;

    ui_screen_t current;
} ui_ctx_t;

static ui_ctx_t ui;

// ---------------------------------------------------------------------------
// 辅助: 加 LVGL 锁 / 安全创建控件
// ---------------------------------------------------------------------------

static bool lock(void)  { return bsp_lvgl_lock(1000); }
static void unlock(void){ bsp_lvgl_unlock(); }

static lv_obj_t *card_create(lv_obj_t *parent, int16_t x, int16_t y,
                             int16_t w, int16_t h, const char *label)
{
    lv_obj_t *c = lv_obj_create(parent);
    lv_obj_set_size(c, w, h);
    lv_obj_set_pos(c, x, y);
    lv_obj_set_style_bg_color(c, lv_color_hex(BG_CARD), 0);
    lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(c, 8, 0);
    lv_obj_set_style_border_width(c, 0, 0);
    lv_obj_set_style_pad_all(c, 6, 0);
    lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
    if (label) {
        lv_obj_t *t = lv_label_create(c);
        lv_label_set_text(t, label);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(INK), 0);
        lv_obj_center(t);
    }
    return c;
}

// ---------------------------------------------------------------------------
// 顶栏: 电量 + 状态
// ---------------------------------------------------------------------------

static void top_bar_create(lv_obj_t *parent)
{
    // 电量条容器
    ui.battery_bar = lv_obj_create(parent);
    lv_obj_set_size(ui.battery_bar, 52, 10);
    lv_obj_set_pos(ui.battery_bar, 240 - 52 - 8, 6);
    lv_obj_set_style_bg_color(ui.battery_bar, lv_color_hex(BATTERY_BG), 0);
    lv_obj_set_style_bg_opa(ui.battery_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(ui.battery_bar, 5, 0);
    lv_obj_set_style_border_width(ui.battery_bar, 0, 0);
    lv_obj_set_style_pad_all(ui.battery_bar, 1, 0);

    ui.battery_fill = lv_obj_create(ui.battery_bar);
    lv_obj_set_size(ui.battery_fill, 48, 6);
    lv_obj_set_style_bg_color(ui.battery_fill, lv_color_hex(ACCENT), 0);
    lv_obj_set_style_radius(ui.battery_fill, 3, 0);
    lv_obj_set_style_border_width(ui.battery_fill, 0, 0);

    ui.battery_pct = lv_label_create(parent);
    lv_obj_set_style_text_font(ui.battery_pct, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui.battery_pct, lv_color_hex(INK_DIM), 0);
    lv_obj_set_pos(ui.battery_pct, 8, 4);
    lv_label_set_text(ui.battery_pct, "---");
}

void treaai_ui_update_battery(int soc)
{
    if (!lock()) return;
    if (soc < 0) {
        lv_label_set_text(ui.battery_pct, "---");
        lv_obj_set_style_bg_color(ui.battery_fill, lv_color_hex(INK_DIM), 0);
    } else {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d%%", soc);
        lv_label_set_text(ui.battery_pct, buf);
        int w = soc * 48 / 100;
        if (w < 4) w = 4;
        lv_obj_set_width(ui.battery_fill, w);
        lv_color_t c = soc > 20 ? lv_color_hex(ACCENT) : lv_color_hex(RED);
        lv_obj_set_style_bg_color(ui.battery_fill, c, 0);
    }
    unlock();
}

void treaai_ui_update_status_label(const char *text)
{
    if (!lock()) return;
    if (ui.home_status_label) {
        lv_label_set_text(ui.home_status_label, text ? text : "就绪");
    }
    unlock();
}

// ---------------------------------------------------------------------------
// Home 屏幕
// ---------------------------------------------------------------------------

static const char *HOME_CARD_LABELS[4] = { "对话", "快捷指令", "设置", "关于" };

static void home_screen_create(void)
{
    ui.scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui.scr, lv_color_hex(BG_DARK), 0);
    lv_obj_set_style_bg_opa(ui.scr, LV_OPA_COVER, 0);
    lv_obj_set_size(ui.scr, 240, 320);

    top_bar_create(ui.scr);

    // Logo "TreaAI"
    ui.home_logo = lv_label_create(ui.scr);
    lv_label_set_text(ui.home_logo, "TreaAI");
    lv_obj_set_style_text_font(ui.home_logo, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(ui.home_logo, lv_color_hex(ACCENT), 0);
    lv_obj_align(ui.home_logo, LV_ALIGN_TOP_MID, 0, 28);

    ui.home_subtitle = lv_label_create(ui.scr);
    lv_label_set_text(ui.home_subtitle, "你的穿戴 AI 伙伴");
    lv_obj_set_style_text_font(ui.home_subtitle, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui.home_subtitle, lv_color_hex(INK_DIM), 0);
    lv_obj_align(ui.home_subtitle, LV_ALIGN_TOP_MID, 0, 52);

    ui.home_status_label = lv_label_create(ui.scr);
    lv_label_set_text(ui.home_status_label, "就绪");
    lv_obj_set_style_text_font(ui.home_status_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui.home_status_label, lv_color_hex(INK_DIM), 0);
    lv_obj_align(ui.home_status_label, LV_ALIGN_TOP_MID, 0, 70);

    // 4 个卡片, 2x2 布局
    int card_w = 100, card_h = 72, gap = 8;
    int start_x = (240 - 2 * card_w - gap) / 2;
    int start_y = 100;
    for (int i = 0; i < 4; i++) {
        int col = i % 2, row = i / 2;
        ui.home_cards[i] = card_create(ui.scr,
            start_x + col * (card_w + gap),
            start_y + row * (card_h + gap),
            card_w, card_h, HOME_CARD_LABELS[i]);
    }
    ui.home_selected = 0;
}

static void home_refresh_selection(void)
{
    for (int i = 0; i < 4; i++) {
        lv_obj_t *c = ui.home_cards[i];
        if (i == ui.home_selected) {
            lv_obj_set_style_bg_color(c, lv_color_hex(ACCENT_DIM), 0);
            lv_obj_set_style_border_width(c, 2, 0);
            lv_obj_set_style_border_color(c, lv_color_hex(ACCENT), 0);
        } else {
            lv_obj_set_style_bg_color(c, lv_color_hex(BG_CARD), 0);
            lv_obj_set_style_border_width(c, 0, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// Chat 屏幕
// ---------------------------------------------------------------------------

static void chat_screen_create(void)
{
    ui.scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui.scr, lv_color_hex(BG_DARK), 0);
    lv_obj_set_style_bg_opa(ui.scr, LV_OPA_COVER, 0);
    lv_obj_set_size(ui.scr, 240, 320);

    top_bar_create(ui.scr);

    // 标题 "对话"
    lv_obj_t *title = lv_label_create(ui.scr);
    lv_label_set_text(title, "对话");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(ACCENT), 0);
    lv_obj_set_pos(title, 8, 22);

    ui.chat_status_line = lv_label_create(ui.scr);
    lv_label_set_text(ui.chat_status_line, "长按 OK 说话");
    lv_obj_set_style_text_font(ui.chat_status_line, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(ui.chat_status_line, lv_color_hex(INK_DIM), 0);
    lv_obj_align(ui.chat_status_line, LV_ALIGN_TOP_MID, 0, 48);

    // 历史列表 (240 x 320 太窄, 用 list 容器)
    ui.chat_history = lv_list_create(ui.scr);
    lv_obj_set_size(ui.chat_history, 224, 320 - 70);
    lv_obj_set_pos(ui.chat_history, 8, 64);
    lv_obj_set_style_bg_color(ui.chat_history, lv_color_hex(BG_CARD), 0);
    lv_obj_set_style_bg_opa(ui.chat_history, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui.chat_history, 0, 0);
    lv_obj_set_style_radius(ui.chat_history, 8, 0);

    // 空列表提示
    lv_obj_t *empty = lv_list_add_text(ui.chat_history, "↑ 这里显示 AI 回复 ↑");
    lv_obj_set_style_text_color(empty, lv_color_hex(INK_DIM), 0);

    ui.chat_msg_count = 0;
}

// ---------------------------------------------------------------------------
// QuickCmd 屏幕
// ---------------------------------------------------------------------------

static void qc_screen_create(void)
{
    ui.scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui.scr, lv_color_hex(BG_DARK), 0);
    lv_obj_set_style_bg_opa(ui.scr, LV_OPA_COVER, 0);
    lv_obj_set_size(ui.scr, 240, 320);

    top_bar_create(ui.scr);

    lv_obj_t *title = lv_label_create(ui.scr);
    lv_label_set_text(title, "快捷指令");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(ACCENT), 0);
    lv_obj_set_pos(title, 8, 22);

    // 6 张卡片, 单列垂直排列, 每张占满宽
    for (int i = 0; i < 6; i++) {
        ui.qc_cards[i] = card_create(ui.scr,
            8, 52 + i * 42, 224, 36,
            treaai_quick_cmds[i].label);
    }
    ui.qc_selected = 0;
}

static void qc_refresh_selection(void)
{
    for (int i = 0; i < 6; i++) {
        lv_obj_t *c = ui.qc_cards[i];
        if (i == ui.qc_selected) {
            lv_obj_set_style_bg_color(c, lv_color_hex(ACCENT_DIM), 0);
            lv_obj_set_style_border_width(c, 2, 0);
            lv_obj_set_style_border_color(c, lv_color_hex(ACCENT), 0);
        } else {
            lv_obj_set_style_bg_color(c, lv_color_hex(BG_CARD), 0);
            lv_obj_set_style_border_width(c, 0, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// Settings 屏幕
// ---------------------------------------------------------------------------

static const char *SETT_LABELS[3] = { "API Key", "API 端点", "测试连接" };

static void sett_screen_create(void)
{
    ui.scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(ui.scr, lv_color_hex(BG_DARK), 0);
    lv_obj_set_style_bg_opa(ui.scr, LV_OPA_COVER, 0);
    lv_obj_set_size(ui.scr, 240, 320);

    top_bar_create(ui.scr);

    lv_obj_t *title = lv_label_create(ui.scr);
    lv_label_set_text(title, "设置");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(ACCENT), 0);
    lv_obj_set_pos(title, 8, 22);

    for (int i = 0; i < 3; i++) {
        ui.sett_cards[i] = card_create(ui.scr,
            8, 56 + i * 52, 224, 44, SETT_LABELS[i]);
    }
    ui.sett_selected = 0;
}

static void sett_refresh_selection(void)
{
    for (int i = 0; i < 3; i++) {
        lv_obj_t *c = ui.sett_cards[i];
        if (i == ui.sett_selected) {
            lv_obj_set_style_bg_color(c, lv_color_hex(ACCENT_DIM), 0);
            lv_obj_set_style_border_width(c, 2, 0);
            lv_obj_set_style_border_color(c, lv_color_hex(ACCENT), 0);
        } else {
            lv_obj_set_style_bg_color(c, lv_color_hex(BG_CARD), 0);
            lv_obj_set_style_border_width(c, 0, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// 公共: 切换屏幕 + 按键事件
// ---------------------------------------------------------------------------

void treaai_ui_init(void)
{
    // 先创建 Home 并 load
    home_screen_create();
    home_refresh_selection();
    ui.current = UI_SCR_HOME;
    lv_screen_load(ui.scr);
}

void treaai_ui_show(ui_screen_t scr)
{
    if (!lock()) return;
    if (ui.scr) {
        lv_obj_delete(ui.scr);
        ui.scr = NULL;
    }
    switch (scr) {
        case UI_SCR_HOME:      home_screen_create();      home_refresh_selection(); break;
        case UI_SCR_CHAT:      chat_screen_create();      break;
        case UI_SCR_QUICK_CMD: qc_screen_create();        qc_refresh_selection();   break;
        case UI_SCR_SETTINGS:  sett_screen_create();      sett_refresh_selection(); break;
    }
    ui.current = scr;
    lv_screen_load(ui.scr);
    unlock();
}

ui_screen_t treaai_ui_current(void) { return ui.current; }

void treaai_ui_on_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!lock()) return;

    // 双击 OK → 回首页 (任何屏幕都生效, Chat 屏等特殊处理除外)
    if (btn == BSP_BTN_OK && ev == BSP_BTN_DOUBLE && ui.current != UI_SCR_HOME) {
        unlock();
        treaai_ui_show(UI_SCR_HOME);
        return;
    }

    switch (ui.current) {
        case UI_SCR_HOME: {
            if (ev == BSP_BTN_CLICK) {
                if (btn == BSP_BTN_UP) ui.home_selected = (ui.home_selected + 3) % 4;
                else if (btn == BSP_BTN_DOWN) ui.home_selected = (ui.home_selected + 1) % 4;
                else if (btn == BSP_BTN_OK) {
                    unlock();
                    if (ui.home_selected == 0) treaai_ui_show(UI_SCR_CHAT);
                    else if (ui.home_selected == 1) treaai_ui_show(UI_SCR_QUICK_CMD);
                    else if (ui.home_selected == 2) treaai_ui_show(UI_SCR_SETTINGS);
                    // 3 = 关于, 暂不实现
                    return;
                }
                home_refresh_selection();
            } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
                // 长按 OK → 直接进对话屏并开始录音
                unlock();
                treaai_ui_show(UI_SCR_CHAT);
                return;
            }
            break;
        }
        case UI_SCR_CHAT: {
            if (ui.chat_status_line) {
                if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
                    lv_label_set_text(ui.chat_status_line, "录音中...");
                } else if (ev == BSP_BTN_CLICK && btn == BSP_BTN_OK) {
                    lv_label_set_text(ui.chat_status_line, "发送中...");
                } else if (ev == BSP_BTN_RELEASE && btn == BSP_BTN_OK) {
                    lv_label_set_text(ui.chat_status_line, "已松开, 发送中");
                }
            }
            break;
        }
        case UI_SCR_QUICK_CMD: {
            if (ev == BSP_BTN_CLICK) {
                if (btn == BSP_BTN_UP) ui.qc_selected = (ui.qc_selected + 5) % 6;
                else if (btn == BSP_BTN_DOWN) ui.qc_selected = (ui.qc_selected + 1) % 6;
                else if (btn == BSP_BTN_OK) {
                    // 发送选中 prompt — 由 app 层处理, 这里暂高亮后等 app 调回复
                    char p[128];
                    treaai_get_quick_prompt(ui.qc_selected, p, sizeof(p));
                    unlock();
                    // 应用层监听选中状态发送 AI 请求
                    extern void treaai_app_on_quick_select(int idx);
                    treaai_app_on_quick_select(ui.qc_selected);
                    return;
                }
                qc_refresh_selection();
            }
            break;
        }
        case UI_SCR_SETTINGS: {
            if (ev == BSP_BTN_CLICK) {
                if (btn == BSP_BTN_UP) ui.sett_selected = (ui.sett_selected + 2) % 3;
                else if (btn == BSP_BTN_DOWN) ui.sett_selected = (ui.sett_selected + 1) % 3;
                sett_refresh_selection();
            }
            break;
        }
    }
    unlock();
}

void treaai_ui_on_ai_reply(const char *text)
{
    if (!text || !*text) return;
    if (!lock()) return;
    if (ui.current == UI_SCR_CHAT && ui.chat_history) {
        // 用户气泡
        char label[256];
        snprintf(label, sizeof(label), "▶ %s", text);
        lv_obj_t *it = lv_list_add_btn(ui.chat_history, NULL, label);
        lv_obj_set_style_text_color(it, lv_color_hex(ACCENT), 0);
        lv_obj_set_style_text_font(it, &lv_font_montserrat_14, 0);
        ui.chat_msg_count++;
        lv_obj_scroll_to_y(ui.chat_history, LV_SCROLL_ALIGN_BOTTOM, LV_ANIM_ON);
    }
    if (ui.chat_status_line) {
        lv_label_set_text(ui.chat_status_line, "OK");
    }
    unlock();
}

void treaai_ui_on_state_change(treaai_state_t state)
{
    if (!lock()) return;
    const char *label = NULL;
    switch (state) {
        case TREAAI_STATE_IDLE:       label = "就绪"; break;
        case TREAAI_STATE_CONNECTING: label = "连接中..."; break;
        case TREAAI_STATE_LISTENING:  label = "录音中..."; break;
        case TREAAI_STATE_THINKING:   label = "AI 思考中..."; break;
        case TREAAI_STATE_SPEAKING:   label = "播放中"; break;
        case TREAAI_STATE_ERROR:      label = "出错了"; break;
    }
    if (ui.home_status_label) {
        lv_label_set_text(ui.home_status_label, label);
    }
    if (ui.chat_status_line && ui.current == UI_SCR_CHAT) {
        lv_label_set_text(ui.chat_status_line, label);
    }
    unlock();
}
