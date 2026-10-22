// main/demo_workbuddy.c —— WorkBuddy 状态页（重新设计 UI）
//
// 特性:
//   - 全新设计（非 demo 像素风），现代深色主题
//   - 状态：空闲/录音中/识别中/显示结果/通知覆盖层
//   - 连接状态指示 + 电池电量
//   - 最近响应文字滚动显示
//   - BLE GATT NUS 协议集成
//
// 触发: 长按 OK → 开始录音，松手发送
//   UP/DOWN/OK → 选项导航（WB_MSG_SHOW_OPTIONS）
//   通知 → 模态覆盖层自动消失
//

#include "demo.h"
#include "demo_workbuddy.h"
#include "demo_radio.h"
#include "ui_pixel.h"
#include "bsp_audio.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "wb_protocol.h"

#include "esp_log.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "nimble/nimble_port.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "lvgl.h"

#include <string.h>

// ============================================================
// 常量
// ============================================================

static const char *TAG = "demo_workbuddy";

// 颜色主题（深色 AI 伴侣风格）
#define C_BG          0x0D1117   // 深背景
#define C_HEADER_BG   0x161B22   // 头部背景
#define C_CARD_BG     0x21262D   // 卡片背景
#define C_BORDER      0x30363D   // 边框
#define C_TEXT_MAIN   0xE6EDF3   // 主文字
#define C_TEXT_MUTED  0x8B949E   // 次要文字
#define C_ACCENT      0x58A6FF   // 强调色（蓝）
#define C_GREEN       0x3FB950   // 连接/成功
#define C_RED         0xF85149   // 断开/错误
#define C_YELLOW      0xD29922   // 处理中
#define C_PURPLE      0xBC8CFF   // 通知

// 屏幕尺寸
#define SCR_W         240
#define SCR_H         320
#define HEADER_H      32
#define FOOTER_H      28
#define CONTENT_Y      (HEADER_H + 8)
#define CONTENT_H     (SCR_H - HEADER_H - FOOTER_H - 8)

// 状态
typedef enum {
    WB_STATE_IDLE = 0,
    WB_STATE_RECORDING,
    WB_STATE_PROCESSING,
    WB_STATE_SHOWING_RESULT,
    WB_STATE_NOTIFICATION,
} wb_state_t;

// ============================================================
// 全局状态
// ============================================================

static bool         s_initialized = false;
static bool         s_gatt_connected = false;
static bool         s_svc_registered = false;  // 服务注册完成后再广告
static uint16_t     s_conn_handle = 0;
static uint16_t     s_rx_char_h = 0;  // BLE GATT TX char value handle（BLE_NUS 惯例）

static wb_state_t    s_state = WB_STATE_IDLE;
static char         s_last_response[512] = {0};  // 最近响应
static uint8_t      s_battery_pct = 0;           // 电池电量
static bool         s_recording = false;
static TaskHandle_t s_rec_task = NULL;

static wb_protocol_t *s_wb = NULL;

// LVGL 对象
static lv_obj_t *s_scr = NULL;
static lv_obj_t *s_header_bg = NULL;
static lv_obj_t *s_conn_dot = NULL;   // 连接状态圆点
static lv_obj_t *s_battery_bar = NULL;
static lv_obj_t *s_state_icon = NULL;  // 状态图标（容器，内含像素块）
static lv_obj_t *s_state_label = NULL;
static lv_obj_t *s_hint_label = NULL;
static lv_obj_t *s_response_label = NULL;
static lv_obj_t *s_notif_overlay = NULL;
static lv_obj_t *s_notif_card = NULL;
static lv_timer_t *s_timer = NULL;
static lv_timer_t *s_notif_timer = NULL;

// BLE
static uint8_t      s_addr_type;
static SemaphoreHandle_t s_host_stopped;
static TaskHandle_t  s_host_task;
static bool         s_stop_in_progress = false;
static bool         s_host_done = false;

// 选项
#define MAX_OPTS  5
#define OPT_LEN   48
static char      s_opts_buf[MAX_OPTS][OPT_LEN]; // 实际存储
static const char *s_opts[MAX_OPTS];             // 指针（初始化后指向上面）
static int  s_opt_cnt = 0;
static int  s_opt_sel = 0;
static lv_obj_t *s_opts_container = NULL;

// ============================================================
// Forward 声明
// ============================================================

static int advertise(void);
static int  gatt_notify_impl(uint16_t conn, const uint8_t *data, size_t len);
static void register_gatt_service(void);
static void on_svc_registered(struct ble_gatt_register_ctxt *ctxt, void *arg);

// ============================================================
// 工具函数
// ============================================================

// 像素块辅助（复用 ui_pixel 的 block）
static lv_obj_t *px_block(lv_obj_t *parent, int x, int y, int w, int h, uint32_t color) {
    lv_obj_t *o = lv_obj_create(parent);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 0, 0);
    lv_obj_set_style_border_width(o, 0, 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_set_style_bg_color(o, lv_color_hex(color), 0);
    return o;
}

static lv_obj_t *px_label(lv_obj_t *parent, int x, int y, const char *txt,
                          const lv_font_t *font, uint32_t color) {
    lv_obj_t *l = lv_label_create(parent);
    lv_obj_set_pos(l, x, y);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    lv_label_set_text(l, txt);
    return l;
}

// ============================================================
// 状态图标绘制（像素块，无图片依赖）
// ============================================================

// 空闲图标：圆点（8×8）
static void draw_icon_idle(lv_obj_t *parent) {
    px_block(parent, 2, 2, 12, 12, C_ACCENT);   // 主体
    px_block(parent, 4, 4,  8,  8, C_BG);    // 中心挖空 = 圆环效果
    // 上半圆弧
    px_block(parent, 5, 0,  6,  2, C_ACCENT);
    // 下半圆弧
    px_block(parent, 5, 14, 6,  2, C_ACCENT);
}

// 录音图标：方块 + 麦克风头（16×16）
static void draw_icon_recording(lv_obj_t *parent) {
    // 麦克风头
    px_block(parent, 5, 1,  6,  8, C_RED);
    px_block(parent, 7, 1,  2,  2, C_BG);    // 振膜
    // 握柄
    px_block(parent, 7, 9,  2,  3, C_RED);
    px_block(parent, 6, 12, 4,  1, C_RED);
    // 底部弧
    px_block(parent, 4, 13, 8,  1, C_RED);
}

// 处理中图标：旋转弧线（简化版，闪烁效果）
static void draw_icon_processing(lv_obj_t *parent) {
    static int s_phase = 0;
    s_phase = (s_phase + 1) % 4;
    uint32_t c = (s_phase == 0) ? C_YELLOW : C_BORDER;
    px_block(parent, 2,  2, 4, 4, c);
    c = (s_phase == 1) ? C_YELLOW : C_BORDER;
    px_block(parent, 10, 2, 4, 4, c);
    c = (s_phase == 2) ? C_YELLOW : C_BORDER;
    px_block(parent, 10, 10, 4, 4, c);
    c = (s_phase == 3) ? C_YELLOW : C_BORDER;
    px_block(parent, 2,  10, 4, 4, c);
    // 中心
    px_block(parent, 6,  6, 4, 4, C_YELLOW);
}

// 结果图标：勾选（16×16）
static void draw_icon_result(lv_obj_t *parent) {
    // L 形
    px_block(parent, 2,  2,  4, 10, C_GREEN);
    px_block(parent, 2, 10,  8,  4, C_GREEN);
    // 斜线
    px_block(parent, 6,  6,  4,  2, C_GREEN);
    px_block(parent, 8,  8,  4,  2, C_GREEN);
    px_block(parent, 10, 10, 2,  2, C_GREEN);
    px_block(parent, 12, 12, 2,  2, C_GREEN);
}

// 通知图标：感叹号（16×16）
static void draw_icon_notify(lv_obj_t *parent, uint32_t color) {
    px_block(parent, 6,  1,  4, 10, color);  // 竖条
    px_block(parent, 6, 13,  4,  2, color);  // 底点
}

static void clear_state_icon(lv_obj_t *parent) {
    lv_obj_clean(parent);
}

// ============================================================
// UI 更新
// ============================================================

static void update_conn_dot(void) {
    if (!s_conn_dot) return;
    lv_obj_set_style_bg_color(s_conn_dot,
        s_gatt_connected ? lv_color_hex(C_GREEN) : lv_color_hex(C_RED), 0);
}

static void update_battery_display(void) {
    if (!s_battery_bar) return;
    int pct = bsp_battery_soc();
    if (pct < 0) pct = s_battery_pct;
    else s_battery_pct = pct;
    // 电池条宽度随电量变化（最大 32px）
    int w = (pct * 32) / 100;
    if (w < 1) w = 1;
    lv_obj_set_size(s_battery_bar, w, 8);
    uint32_t color = (pct > 20) ? C_GREEN : C_RED;
    lv_obj_set_style_bg_color(s_battery_bar, lv_color_hex(color), 0);
}

static void update_state_icon(void) {
    if (!s_state_icon) return;
    clear_state_icon(s_state_icon);

    switch (s_state) {
        case WB_STATE_IDLE:
            draw_icon_idle(s_state_icon);
            break;
        case WB_STATE_RECORDING:
            draw_icon_recording(s_state_icon);
            break;
        case WB_STATE_PROCESSING:
            draw_icon_processing(s_state_icon);
            break;
        case WB_STATE_SHOWING_RESULT:
            draw_icon_result(s_state_icon);
            break;
        case WB_STATE_NOTIFICATION:
            draw_icon_notify(s_state_icon, C_PURPLE);
            break;
    }
}

static void update_state_label(void) {
    if (!s_state_label) return;
    const char *txt;
    switch (s_state) {
        case WB_STATE_IDLE:
            txt = s_gatt_connected ? "Hold OK to record" : "Waiting for PC Bridge...";
            break;
        case WB_STATE_RECORDING:
            txt = "RECORDING...";
            break;
        case WB_STATE_PROCESSING:
            txt = "Processing...";
            break;
        case WB_STATE_SHOWING_RESULT:
            txt = "Sent OK";
            break;
        case WB_STATE_NOTIFICATION:
            txt = "";
            break;
        default:
            txt = "";
            break;
    }
    lv_label_set_text(s_state_label, txt);
}

static void update_hint_label(void) {
    if (!s_hint_label) return;
    const char *txt = "";
    switch (s_state) {
        case WB_STATE_IDLE:
            txt = "Release to send";
            break;
        case WB_STATE_RECORDING:
            txt = "Release to send";
            break;
        case WB_STATE_PROCESSING:
            txt = "";
            break;
        case WB_STATE_SHOWING_RESULT:
            txt = "UP/DOWN: history";
            break;
        default:
            txt = "";
    }
    lv_label_set_text(s_hint_label, txt);
}

static void update_response_display(void) {
    if (!s_response_label) return;
    if (s_last_response[0]) {
        // 取最后两行
        const char *p = strrchr(s_last_response, '\n');
        const char *show = p ? (p + 1) : s_last_response;
        // 截断到 100 字符
        static char buf[128];
        int len = strlen(show);
        if (len > 100) {
            memcpy(buf, show, 97);
            buf[97] = '\0';
            strcat(buf, "...");
        } else {
            strcpy(buf, show);
        }
        lv_label_set_text(s_response_label, buf);
        lv_obj_remove_flag(s_response_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_label_set_text(s_response_label, "No response yet");
    }
}

static void refresh_ui(void) {
    update_conn_dot();
    update_battery_display();
    update_state_icon();
    update_state_label();
    update_hint_label();
    update_response_display();
}

// ============================================================
// 通知覆盖层
// ============================================================

static void dismiss_notification_overlay(void);  // 前向声明
static void notif_timer_cb(lv_timer_t *t) {
    (void)t;
    dismiss_notification_overlay();
}

static void show_notification_overlay(const char *title, const char *body,
                                   const char *priority) {
    if (s_notif_overlay) return;

    // 全屏遮罩
    s_notif_overlay = lv_obj_create(s_scr);
    lv_obj_set_size(s_notif_overlay, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(s_notif_overlay, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_notif_overlay, LV_OPA_60, 0);
    lv_obj_remove_flag(s_notif_overlay, LV_OBJ_FLAG_SCROLLABLE);

    // 居中卡片
    s_notif_card = lv_obj_create(s_notif_overlay);
    lv_obj_set_size(s_notif_card, 200, 140);
    lv_obj_set_pos(s_notif_card, 20, 80);
    lv_obj_set_style_bg_color(s_notif_card, lv_color_hex(C_CARD_BG), 0);
    lv_obj_set_style_radius(s_notif_card, 8, 0);
    lv_obj_set_style_border_width(s_notif_card, 2, 0);
    lv_obj_set_style_border_color(s_notif_card, lv_color_hex(C_PURPLE), 0);

    // 优先级色条
    uint32_t bar_color = C_PURPLE;
    if (priority && strcmp(priority, "high") == 0) bar_color = C_RED;
    else if (priority && strcmp(priority, "low") == 0) bar_color = C_BORDER;
    px_block(s_notif_card, 0, 0, 200, 4, bar_color);

    // 标题
    lv_obj_t *tl = lv_label_create(s_notif_card);
    lv_obj_set_pos(tl, 12, 14);
    lv_obj_set_style_text_font(tl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(tl, lv_color_hex(C_TEXT_MAIN), 0);
    lv_label_set_text(tl, title ? title : "Notification");

    // 正文（滚动支持）
    lv_obj_t *bl = lv_label_create(s_notif_card);
    lv_obj_set_pos(bl, 12, 46);
    lv_obj_set_size(bl, 176, 80);
    lv_obj_set_style_text_font(bl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(bl, lv_color_hex(C_TEXT_MUTED), 0);
    lv_label_set_long_mode(bl, LV_LABEL_LONG_SCROLL);
    lv_label_set_text(bl, body ? body : "");

    // 底部提示
    lv_obj_t *fl = lv_label_create(s_notif_card);
    lv_obj_set_pos(fl, 12, 120);
    lv_obj_set_style_text_font(fl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(fl, lv_color_hex(C_TEXT_MUTED), 0);
    lv_label_set_text(fl, "OK: close");

    // 自动消失（普通 5 秒，高优先级不自动消失）
    int auto_ms = 5000;
    if (priority && (strcmp(priority, "high") == 0 || strcmp(priority, "urgent") == 0)) {
        auto_ms = 0;
    }
    if (auto_ms > 0) {
        s_notif_timer = lv_timer_create(notif_timer_cb, auto_ms, NULL);
    }
}

static void dismiss_notification_overlay(void) {
    if (s_notif_timer) {
        lv_timer_delete(s_notif_timer);
        s_notif_timer = NULL;
    }
    if (s_notif_overlay) {
        lv_obj_del(s_notif_overlay);
        s_notif_overlay = NULL;
        s_notif_card = NULL;
    }
}

// ============================================================
// 选项 UI（内嵌在内容区）
// ============================================================

static void show_options_ui(const char *json) {
    if (s_opts_container) {
        lv_obj_del(s_opts_container);
        s_opts_container = NULL;
    }

    // 解析选项（简化：提取 "text" 字段）
    s_opt_cnt = 0;
    s_opt_sel = 0;
    // 初始化选项指针
    for (int i = 0; i < MAX_OPTS; i++) s_opts[i] = s_opts_buf[i];
    const char *p = json;
    while ((p = strstr(p, "\"text\":")) && s_opt_cnt < MAX_OPTS) {
        p = strchr(p, ':');
        if (!p) break;
        p++;
        while (*p == ' ' || *p == '"') p++;
        const char *end = p;
        while (*end && *end != '"' && *end != ',') end++;
        size_t len = (size_t)(end - p);
        if (len > 0 && len < OPT_LEN) {
            memcpy((char *)s_opts[s_opt_cnt], p, len);
            ((char *)s_opts[s_opt_cnt])[len] = '\0';
            s_opt_cnt++;
        }
        p = end;
    }

    if (s_opt_cnt == 0) return;

    // 绘制选项列表
    s_opts_container = lv_obj_create(s_scr);
    lv_obj_set_pos(s_opts_container, 10, CONTENT_Y + 40);
    lv_obj_set_size(s_opts_container, SCR_W - 20, CONTENT_H - 40);
    lv_obj_set_style_bg_color(s_opts_container, lv_color_hex(C_CARD_BG), 0);
    lv_obj_set_style_radius(s_opts_container, 6, 0);
    lv_obj_remove_flag(s_opts_container, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < s_opt_cnt; i++) {
        int y = 8 + i * 38;
        uint32_t bg = (i == s_opt_sel) ? C_ACCENT : C_HEADER_BG;
        uint32_t fg = (i == s_opt_sel) ? C_BG : C_TEXT_MAIN;

        lv_obj_t *row = lv_obj_create(s_opts_container);
        lv_obj_set_pos(row, 6, y);
        lv_obj_set_size(row, SCR_W - 20 - 12, 34);
        lv_obj_set_style_bg_color(row, lv_color_hex(bg), 0);
        lv_obj_set_style_radius(row, 4, 0);

        lv_obj_t *arrow = lv_label_create(row);
        lv_obj_set_pos(arrow, 6, 8);
        lv_obj_set_style_text_font(arrow, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(arrow, lv_color_hex(fg), 0);
        lv_label_set_text(arrow, i == s_opt_sel ? "▶" : " ");

        lv_obj_t *txt = lv_label_create(row);
        lv_obj_set_pos(txt, 24, 8);
        lv_obj_set_size(txt, SCR_W - 40, 20);
        lv_obj_set_style_text_font(txt, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(txt, lv_color_hex(fg), 0);
        // 截断超长文字
        static char opt_buf[64];
        const char *src = s_opts[i] ? s_opts[i] : "";
        int len = strlen(src);
        if (len > 28) {
            memcpy(opt_buf, src, 25);
            opt_buf[25] = '\0';
            strcat(opt_buf, "...");
            src = opt_buf;
        }
        lv_label_set_text(txt, src);
    }
}

static void clear_options_ui(void) {
    s_opt_cnt = 0;
    s_opt_sel = 0;
    if (s_opts_container) {
        lv_obj_del(s_opts_container);
        s_opts_container = NULL;
    }
}

static void options_nav(int delta) {
    if (s_opt_cnt == 0) return;
    s_opt_sel = (s_opt_sel + delta + s_opt_cnt) % s_opt_cnt;
    // 重新绘制（简化：重建）
    show_options_ui("{}");
}

static void confirm_option(void) {
    if (s_opt_cnt == 0) return;
    const char *sel = s_opts[s_opt_sel];
    ESP_LOGI(TAG, "确认选项: %s", sel ? sel : "");

    char id[16];
    snprintf(id, sizeof(id), "%d", s_opt_sel + 1);
    wb_send_option_selected(s_wb, id);

    clear_options_ui();
}

// ============================================================
// 协议回调
// ============================================================

static void on_notification(wb_protocol_t *p, const char *title,
                             const char *body, const char *priority) {
    (void)p;
    ESP_LOGI(TAG, "通知: %s", title ? title : "");
    s_state = WB_STATE_NOTIFICATION;
    show_notification_overlay(title, body, priority);
}

static void on_show_options(wb_protocol_t *p, const char *json) {
    (void)p;
    ESP_LOGI(TAG, "显示选项: %s", json);
    clear_options_ui();
    show_options_ui(json);
}

static void on_key_acked(wb_protocol_t *p, const char *button,
                        const char *event, bool success) {
    (void)p;
    ESP_LOGI(TAG, "按键 ACK: %s/%s success=%d", button, event, success);
}

static void on_clear_screen(wb_protocol_t *p) {
    (void)p;
    dismiss_notification_overlay();
    clear_options_ui();
    s_state = WB_STATE_IDLE;
    refresh_ui();
}

static void on_voice_text(wb_protocol_t *p, const char *text) {
    (void)p;
    if (!text) return;
    if (text[0] == '\0') return;  // 录音结束，不处理

    // 注入文字确认，显示在 response 区
    strncpy(s_last_response, text, sizeof(s_last_response) - 1);
    s_last_response[sizeof(s_last_response) - 1] = '\0';
    s_state = WB_STATE_SHOWING_RESULT;
    refresh_ui();
}

// ============================================================
// 录音任务
// ============================================================

#define PCM_CHUNK  2048
#define AUDIO_HZ   16000
#define AUDIO_BITS  16
#define AUDIO_CH    1
#define MAX_AUDIO   (AUDIO_HZ * AUDIO_BITS / 8 * 30)  // 30s

static uint8_t *s_audio_buf = NULL;
static size_t   s_audio_len = 0;

static void audio_rec_task(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "录音任务启动");

    if (!s_audio_buf) {
        s_audio_buf = malloc(MAX_AUDIO);
    }
    s_audio_len = 0;

    esp_err_t err = bsp_audio_set_format(AUDIO_HZ, AUDIO_BITS, AUDIO_CH);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "音频格式失败: %s", esp_err_to_name(err));
        s_recording = false;
        vTaskDelete(NULL);
        return;
    }

    s_state = WB_STATE_RECORDING;
    refresh_ui();

    uint8_t pcm[PCM_CHUNK];
    while (s_recording && s_audio_len + PCM_CHUNK < MAX_AUDIO) {
        int rc = bsp_audio_read(pcm, sizeof(pcm));
        if (rc == ESP_OK) {
            memcpy(s_audio_buf + s_audio_len, pcm, PCM_CHUNK);
            s_audio_len += PCM_CHUNK;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    ESP_LOGI(TAG, "录音结束: %d bytes", s_audio_len);

    s_state = WB_STATE_PROCESSING;
    refresh_ui();

    if (s_audio_len > 0 && s_gatt_connected) {
        // 发送元数据
        char meta[128];
        snprintf(meta, sizeof(meta),
                 "{\"type\":\"wb_voice_end\",\"payload\":{\"total_bytes\":%zu,\"duration\":%.1f}}\n",
                 s_audio_len, (double)s_audio_len / (AUDIO_HZ * AUDIO_BITS / 8));
        gatt_notify_impl(s_conn_handle, (const uint8_t *)meta, strlen(meta));
        vTaskDelay(pdMS_TO_TICKS(30));

        // 分包发送 hex
        const size_t hex_per_chunk = 200;
        char hex_pkt[512];
        size_t sent = 0;
        int seq = 0;
        while (sent < s_audio_len) {
            size_t chunk = s_audio_len - sent;
            if (chunk > hex_per_chunk) chunk = hex_per_chunk;
            char hex[hex_per_chunk * 2 + 4];
            for (size_t i = 0; i < chunk; i++) {
                snprintf(hex + i * 2, 3, "%02x", s_audio_buf[sent + i]);
            }
            snprintf(hex_pkt, sizeof(hex_pkt),
                     "{\"type\":\"wb_voice_chunk\",\"payload\":{\"seq\":%d,\"hex\":\"%s\"}}\n",
                     seq++, hex);
            gatt_notify_impl(s_conn_handle, (const uint8_t *)hex_pkt, strlen(hex_pkt));
            sent += chunk;
            vTaskDelay(pdMS_TO_TICKS(20));
        }
        ESP_LOGI(TAG, "音频发送完成: %d bytes, %d 包", s_audio_len, seq);
    } else if (s_audio_len == 0) {
        ESP_LOGW(TAG, "无录音数据");
    }

    s_audio_len = 0;
    s_recording = false;
    s_rec_task = NULL;
    vTaskDelete(NULL);
}

static void toggle_recording(void) {
    if (s_recording) {
        s_recording = false;
    } else {
        if (!s_gatt_connected) {
            ESP_LOGW(TAG, "GATT 未连接");
            return;
        }
        if (!bsp_audio_init()) {
            ESP_LOGE(TAG, "音频初始化失败");
            return;
        }
        s_recording = true;
        xTaskCreatePinnedToCore(&audio_rec_task, "wb_rec", 4096, NULL,
                                5, &s_rec_task, PRO_CPU_NUM);
    }
}

// ============================================================
// BLE GATT
// ============================================================

static int gatt_notify_impl(uint16_t conn, const uint8_t *data, size_t len) {
    if (!s_gatt_connected) return ESP_ERR_INVALID_STATE;
    struct os_mbuf *om = ble_hs_mbuf_att_pkt();
    if (!om) return ESP_ERR_NO_MEM;
    int rc = os_mbuf_append(om, data, len);
    if (rc != 0) { os_mbuf_free_chain(om); return ESP_ERR_NO_MEM; }
    rc = ble_gattc_notify_custom(conn, s_rx_char_h, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

static int gatt_access_cb(uint16_t conn_handle, uint16_t attr_handle,
                         struct ble_gatt_access_ctxt *ctxt, void *arg) {
    (void)conn_handle; (void)attr_handle; (void)arg;
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        struct os_mbuf *om = ctxt->om;
        if (om && OS_MBUF_PKTLEN(om) > 0) {
            uint8_t buf[512];
            uint16_t len = OS_MBUF_PKTLEN(om);
            if (len > sizeof(buf) - 1) len = sizeof(buf) - 1;
            os_mbuf_copydata(om, 0, len, buf);
            buf[len] = '\0';
            wb_protocol_on_rx_data(s_wb, buf, len);
        }
        return 0;
    }
    if (ctxt->op == BLE_GATT_ACCESS_OP_READ_CHR) return 0;
    return 0;
}

static int gap_event(struct ble_gap_event *event, void *arg) {
    (void)arg;
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            ESP_LOGI(TAG, "[GAP] CONNECT conn_handle=%d", event->connect.conn_handle);
            s_gatt_connected = true;
            s_conn_handle = event->connect.conn_handle;
            wb_protocol_on_conn_change(s_wb, true, s_conn_handle);
            ble_gap_adv_stop();
            s_state = WB_STATE_IDLE;
            refresh_ui();
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            ESP_LOGI(TAG, "[GAP] DISCONNECT reason=%d", event->disconnect.reason);
            s_gatt_connected = false;
            s_conn_handle = 0;
            wb_protocol_on_conn_change(s_wb, false, 0);
            if (s_recording) {
                s_recording = false;
                if (s_rec_task) { vTaskDelete(s_rec_task); s_rec_task = NULL; }
            }
            clear_options_ui();
            dismiss_notification_overlay();
            s_state = WB_STATE_IDLE;
            if (s_initialized && s_svc_registered) advertise();
            refresh_ui();
            break;

        case BLE_GAP_EVENT_ADV_COMPLETE:
            ESP_LOGI(TAG, "[GAP] ADV_COMPLETE");
            if (s_initialized && !s_gatt_connected && s_svc_registered) advertise();
            break;

        default:
            break;
    }
    return 0;
}

static int advertise(void) {
    ESP_LOGI(TAG, "[GAP] advertise() called");
    struct ble_hs_adv_fields fields = { 0 };
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)"FoloPassport";
    fields.name_len = strlen("FoloPassport");
    fields.name_is_complete = 1;
    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) { ESP_LOGE(TAG, "adv_set_fields failed: %d", rc); return rc; }

    struct ble_gap_adv_params params = { 0 };
    params.conn_mode = BLE_GAP_CONN_MODE_UND;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    params.itvl_min = BLE_GAP_ADV_ITVL_MS(100);
    params.itvl_max = BLE_GAP_ADV_ITVL_MS(100);
    return ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &params,
                              gap_event, NULL);
}

static void register_gatt_service(void) {
    static const ble_uuid128_t svc_uuid   = BLE_UUID128_INIT(
        0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,
        0x93,0xf3,0xa3,0xb5,0x01,0x00,0x00,0x6e);
    static const ble_uuid128_t tx_uuid    = BLE_UUID128_INIT(
        0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,
        0x93,0xf3,0xa3,0xb5,0x02,0x00,0x00,0x6e);
    static const ble_uuid128_t rx_uuid    = BLE_UUID128_INIT(
        0x9e,0xca,0xdc,0x24,0x0e,0xe5,0xa9,0xe0,
        0x93,0xf3,0xa3,0xb5,0x03,0x00,0x00,0x6e);

    static struct ble_gatt_chr_def chr_defs[] = {
        {
            .uuid = (const ble_uuid_t *)&tx_uuid,
            .access_cb = gatt_access_cb,
            .flags = BLE_GATT_CHR_F_WRITE_NO_RSP | BLE_GATT_CHR_F_WRITE,
            .min_key_size = 16,
            .val_handle = &s_rx_char_h,
        },
        {
            .uuid = (const ble_uuid_t *)&rx_uuid,
            .access_cb = gatt_access_cb,
            .flags = BLE_GATT_CHR_F_NOTIFY,
            .min_key_size = 16,
        },
        { 0 },
    };
    static const struct ble_gatt_svc_def svcs[] = {
        { BLE_GATT_SVC_TYPE_PRIMARY, (const ble_uuid_t *)&svc_uuid, .characteristics = chr_defs },
        { 0 },
    };

    // 设置回调：ble_gatts_add_svcs 内部会调用此回调，等服务真正注册后再广告
    ble_hs_cfg.gatts_register_cb = on_svc_registered;
    (void)ble_gatts_add_svcs(svcs);
}

// GATT 服务注册完成回调——等服务真正注册后再开始广播
static void on_svc_registered(struct ble_gatt_register_ctxt *ctxt, void *arg) {
    (void)arg;
    if (ctxt->op == BLE_GATT_REGISTER_OP_SVC) {
        ESP_LOGI(TAG, "[GATT] svc registered: handle=%d", ctxt->svc.handle);
        s_svc_registered = true;
        ESP_LOGI(TAG, "[GATT] starting advertise after svc registered");
        int rc = advertise();
        if (rc != 0) ESP_LOGE(TAG, "advertise failed: %d", rc);
    } else if (ctxt->op == BLE_GATT_REGISTER_OP_CHR) {
        ESP_LOGI(TAG, "[GATT] chr registered: def_handle=%d val_handle=%d",
                 ctxt->chr.def_handle, ctxt->chr.val_handle);
    }
}

static void on_reset(int reason) {
    ESP_LOGE(TAG, "NimBLE reset: %d", reason);
}
static void on_sync(void) {
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) rc = ble_hs_id_infer_auto(0, &s_addr_type);
    if (rc == 0 && s_initialized) {
        register_gatt_service();
        // 不在这里广告——等 gatts_register_cb 回调
        ESP_LOGI(TAG, "[GAP] on_sync done, waiting for svc registration...");
    }
    if (rc != 0) ESP_LOGE(TAG, "on_sync 失败: %d", rc);
}
static void host_task(void *arg) {
    (void)arg;
    nimble_port_run();
    xSemaphoreGive(s_host_stopped);
    for (;;) vTaskSuspend(NULL);
}

// ============================================================
// 公开 API
// ============================================================

esp_err_t demo_workbuddy_start(void) {
    if (s_initialized) return ESP_ERR_INVALID_STATE;
    esp_err_t err = demo_radio_nvs_prepare();
    if (err != ESP_OK) return err;
    err = nimble_port_init();
    if (err != ESP_OK) return err;

    s_initialized = true;
    s_stop_in_progress = false;
    s_host_done = false;
    s_gatt_connected = false;
    s_svc_registered = false;
    s_recording = false;
    s_state = WB_STATE_IDLE;
    s_last_response[0] = '\0';

    s_wb = wb_protocol_new();
    if (!s_wb) return ESP_ERR_NO_MEM;
    wb_protocol_set_user_data(s_wb, NULL);
    wb_protocol_set_gatt_notify_cb(gatt_notify_impl);
    s_wb->on_voice_text = on_voice_text;
    s_wb->on_show_options = on_show_options;
    s_wb->on_notification = on_notification;
    s_wb->on_key_acked = on_key_acked;
    s_wb->on_clear_screen = on_clear_screen;

    ble_svc_gap_init();
    ble_svc_gatt_init();
    ble_svc_gap_device_name_set("FoloPassport");
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    s_host_stopped = xSemaphoreCreateBinary();
    if (!s_host_stopped) return ESP_ERR_NO_MEM;
    if (xTaskCreatePinnedToCore(host_task, "wb_host", NIMBLE_HS_STACK_SIZE,
                                NULL, configMAX_PRIORITIES - 4, &s_host_task,
                                NIMBLE_CORE) != pdPASS) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t demo_workbuddy_stop(void) {
    s_initialized = false;
    if (s_recording) {
        s_recording = false;
        if (s_rec_task) { vTaskDelete(s_rec_task); s_rec_task = NULL; }
    }
    if (s_host_task && !s_stop_in_progress) {
        (void)ble_gap_adv_stop();
        int rc = nimble_port_stop();
        if (rc != 0) return ESP_FAIL;
        s_stop_in_progress = true;
    }
    if (s_host_task && !s_host_done) {
        if (xSemaphoreTake(s_host_stopped, pdMS_TO_TICKS(2000)) != pdTRUE) {
            ESP_LOGE(TAG, "NimBLE 停止超时");
        } else {
            s_host_done = true;
        }
    }
    if (s_host_task) { vTaskDelete(s_host_task); s_host_task = NULL; }
    esp_err_t err = nimble_port_deinit();
    if (s_wb) { wb_protocol_free(s_wb); s_wb = NULL; }
    if (s_audio_buf) { free(s_audio_buf); s_audio_buf = NULL; }
    if (s_host_stopped) { vSemaphoreDelete(s_host_stopped); s_host_stopped = NULL; }
    s_stop_in_progress = false;
    s_host_done = false;
    return err;
}

// ============================================================
// LVGL 定时器
// ============================================================

static void tick(lv_timer_t *timer) {
    (void)timer;
    if (!s_initialized) return;

    // 录音图标闪烁
    if (s_state == WB_STATE_RECORDING) {
        static int blink = 0;
        blink ^= 1;
        if (blink) {
            lv_obj_set_style_bg_color(s_state_icon, lv_color_hex(C_RED), 0);
        } else {
            lv_obj_set_style_bg_color(s_state_icon, lv_color_hex(C_HEADER_BG), 0);
        }
    }
    // 处理中动画
    if (s_state == WB_STATE_PROCESSING) {
        update_state_icon();  // 每 500ms 更新一帧
    }
}

// ============================================================
// LVGL 页面
// ============================================================

void demo_workbuddy_enter(void) {
    // 背景
    s_scr = lv_obj_create(NULL);
    lv_obj_set_size(s_scr, SCR_W, SCR_H);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(C_BG), 0);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    // ── Header ──────────────────────────────────────────
    s_header_bg = px_block(s_scr, 0, 0, SCR_W, HEADER_H, C_HEADER_BG);

    // 连接状态圆点
    s_conn_dot = lv_obj_create(s_header_bg);
    lv_obj_set_pos(s_conn_dot, 10, 12);
    lv_obj_set_size(s_conn_dot, 10, 10);
    lv_obj_set_style_radius(s_conn_dot, 5, 0);
    lv_obj_set_style_bg_color(s_conn_dot, lv_color_hex(C_RED), 0);
    lv_obj_set_style_border_width(s_conn_dot, 0, 0);

    lv_obj_t *title = px_label(s_header_bg, 26, 8, "WorkBuddy",
                                  &lv_font_montserrat_14, C_TEXT_MAIN);
    (void)title;  // 已作背景装饰，保留语义
    px_block(s_header_bg, SCR_W - 54, 8, 36, 14, C_BORDER);
    px_block(s_header_bg, SCR_W - 58, 11, 6, 8, C_BORDER);  // 电池帽
    // 电池条（动态）
    s_battery_bar = px_block(s_header_bg, SCR_W - 52, 10, 32, 10, C_GREEN);

    // 分隔线
    px_block(s_scr, 0, HEADER_H, SCR_W, 1, C_BORDER);

    // ── 状态图标（像素块，16×16，居中）──────────────────
    s_state_icon = lv_obj_create(s_scr);
    lv_obj_set_pos(s_state_icon, (SCR_W - 16) / 2, CONTENT_Y + 10);
    lv_obj_set_size(s_state_icon, 16, 16);
    lv_obj_set_style_bg_color(s_state_icon, lv_color_hex(C_HEADER_BG), 0);
    lv_obj_set_style_radius(s_state_icon, 2, 0);
    lv_obj_remove_flag(s_state_icon, LV_OBJ_FLAG_SCROLLABLE);

    // 状态文字
    s_state_label = px_label(s_scr, 0, CONTENT_Y + 32,
                              "长按 OK 开始录音",
                              &lv_font_montserrat_14, C_TEXT_MAIN);
    lv_obj_set_align(s_state_label, LV_ALIGN_TOP_MID);

    // 提示文字
    s_hint_label = px_label(s_scr, 0, CONTENT_Y + 52,
                              "松开自动发送",
                              &lv_font_montserrat_14, C_TEXT_MUTED);
    lv_obj_set_align(s_hint_label, LV_ALIGN_TOP_MID);

    // ── 最近响应区 ────────────────────────────────────
    int resp_y = CONTENT_Y + 76;
    px_block(s_scr, 10, resp_y, SCR_W - 20, 1, C_BORDER);
    lv_obj_t *resp_title = px_label(s_scr, 10, resp_y + 4, "Response",
                                    &lv_font_montserrat_14, C_TEXT_MUTED);
    (void)resp_title;

    s_response_label = px_label(s_scr, 10, resp_y + 20, "No response yet",
                                &lv_font_montserrat_14, C_TEXT_MAIN);
    lv_obj_set_size(s_response_label, SCR_W - 20, CONTENT_H - (resp_y + 20 - CONTENT_Y) - 4);
    lv_label_set_long_mode(s_response_label, LV_LABEL_LONG_DOT);

    // ── 底部提示 ────────────────────────────────────
    int footer_y = SCR_H - FOOTER_H;
    px_block(s_scr, 0, footer_y, SCR_W, 1, C_BORDER);
    lv_obj_t *footer = px_label(s_scr, 0, footer_y + 6,
                                  "OK:record  UP/DOWN:options",
                                  &lv_font_montserrat_14, C_TEXT_MUTED);
    lv_obj_set_align(footer, LV_ALIGN_TOP_MID);

    // ── 定时器 ───────────────────────────────────────
    s_timer = lv_timer_create(tick, 500, NULL);

    // 初始状态
    s_battery_pct = bsp_battery_soc();
    if (s_battery_pct == 0) s_battery_pct = 85;
    refresh_ui();

    lv_screen_load(s_scr);
}

void demo_workbuddy_exit(void) {
    if (s_timer) { lv_timer_delete(s_timer); s_timer = NULL; }
    dismiss_notification_overlay();
    clear_options_ui();
    if (s_recording) {
        s_recording = false;
        if (s_rec_task) { vTaskDelete(s_rec_task); s_rec_task = NULL; }
    }
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; }
    s_conn_dot = NULL;
    s_battery_bar = NULL;
    s_state_icon = NULL;
    s_state_label = NULL;
    s_hint_label = NULL;
    s_response_label = NULL;
    s_opts_container = NULL;
}

void demo_workbuddy_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    // 通知层：OK 关闭
    if (s_notif_overlay && btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
        dismiss_notification_overlay();
        s_state = WB_STATE_IDLE;
        refresh_ui();
        return;
    }

    // 选项导航
    if (s_opt_cnt > 0) {
        if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
            options_nav(-1);
            wb_send_key_event(s_wb, "up", "press");
        } else if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
            options_nav(+1);
            wb_send_key_event(s_wb, "down", "press");
        } else if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK) {
            confirm_option();
            wb_send_key_event(s_wb, "ok", "press");
        }
        return;
    }

    // OK 键：录音控制
    if (btn == BSP_BTN_OK) {
        if (ev == BSP_BTN_PRESS && !s_recording && s_gatt_connected) {
            toggle_recording();
        } else if (ev == BSP_BTN_RELEASE && s_recording) {
            toggle_recording();
        }
        return;
    }

    // 非录音状态下的 UP/DOWN：转发键盘事件（供 WorkBuddy 弹窗用）
    if (!s_recording && s_gatt_connected) {
        if (btn == BSP_BTN_UP && ev == BSP_BTN_CLICK) {
            wb_send_key_event(s_wb, "up", "press");
        } else if (btn == BSP_BTN_DOWN && ev == BSP_BTN_CLICK) {
            wb_send_key_event(s_wb, "down", "press");
        }
    }
}
