// main/passport_ui.c —— 六个屏幕的 LVGL 实现（MENU/IDENTITY/QR/POINTS/CHECKIN/SETTINGS）。
//
// 只运行在 UI 上下文（LVGL 任务）。数据一律读 NVS 缓存与 passport_net 的
// 快照接口；联网动作只发 passport_net_request，不在此等待。
//
// QR 与像素头像用 LV_COLOR_FORMAT_I1 画布直接写位（模块锐利、内存极小）：
//   QR 37x37 模块 × 6 = 222px 画布，约 6.2 KB；
//   头像 8x8 × 8 = 64px 画布，512 B。
#include "passport_ui.h"

#include "passport_core.h"
#include "passport_net.h"
#include "passport_store.h"
#include "ui_pixel.h"
#include "qrcodegen.h"

#include "bsp_battery.h"
#include "bsp_button.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_system.h"
#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "smpass_ui";

// 中文字体由 tools/gen_font.sh 生成（font_sm_cjk_16.c），仅含 UI 用到的字。
// 它是 lv_font_conv 的 RLE 压缩位图，需 CONFIG_LV_USE_FONT_COMPRESSED=y 才能渲染。
// s_font_cjk 在原有字体上挂 LVGL 内置思源黑体（1000 常用字）作 fallback，
// 覆盖服务端下发的姓名/积分文本等无法预先子集化的字符。
extern const lv_font_t font_sm_cjk_16;
extern const lv_font_t lv_font_source_han_sans_sc_16_cjk;

static lv_font_t s_font_cjk;

#define FONT_CJK   (&s_font_cjk)
#define FONT_LATIN (&lv_font_montserrat_14)
#define FONT_BIG   (&lv_font_montserrat_20)

#define QR_PX        222                 // 屏上二维码边长（模块 6px）
#define QR_STRIDE    ((QR_PX + 7) / 8)   // I1 行字节数
#define AVATAR_PX    64
#define AVATAR_STRIDE ((AVATAR_PX + 7) / 8)

#define DEMO_HOLD_EXTRA_MS 3500          // LONG 事件后再按住这么久即满 5 秒

static QueueHandle_t s_ui_queue;

static lv_obj_t *s_scr;
static passport_screen_t s_current = PASSPORT_SCREEN_MENU;
static int s_sel;                        // MENU/SETTINGS 选中行

// 各屏需要持续引用的对象
static lv_obj_t *s_rows[6];              // 菜单/设置行
static lv_obj_t *s_status;               // 状态行（pid + 在馆标记 + 电量）
static lv_obj_t *s_body;                 // 正文主标签
static lv_obj_t *s_body2;                // 副标签
static lv_obj_t *s_canvas;               // QR / 头像画布
static lv_obj_t *s_bar;                  // QR 倒计时条
static uint8_t *s_canvas_buf;
static uint32_t s_qr_window;             // 当前已绘制的二维码窗口
static bool s_sync_ever_ok;              // 本轮开机后是否同步成功过（离线角标用）
static lv_timer_t *s_hold_timer;         // 设置页 5 秒长按检测
static bool s_factory_armed;             // 恢复出厂二次确认状态

// ---------------------------------------------------------------------------
// 小工具
// ---------------------------------------------------------------------------
static void get_pid(char *out, size_t len)
{
    if (passport_store_get_string(PASSPORT_KEY_PID, out, len) != ESP_OK) {
        strlcpy(out, "--", len);
    }
}

static uint32_t now_ts(void)
{
    return (uint32_t)time(NULL);
}

// 状态栏：左 pid + 在馆标记；右上电量（避开白云装饰，用其下方蓝天区）
static void add_status_bar(lv_obj_t *scr)
{
    char pid[32];
    get_pid(pid, sizeof(pid));
    uint8_t in_lab = 0;
    passport_store_get_u8(PASSPORT_KEY_IN_LAB, &in_lab);

    s_status = lv_label_create(scr);
    lv_obj_set_style_text_font(s_status, FONT_CJK, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(UI_PAPER), 0);
    lv_label_set_text_fmt(s_status, "%s%s", pid, in_lab ? "  在馆中" : "");
    lv_obj_set_pos(s_status, 10, 47);

    int soc = bsp_battery_soc();
    if (soc >= 0) {
        lv_obj_t *bat = lv_label_create(scr);
        lv_obj_set_style_text_font(bat, FONT_LATIN, 0);
        lv_obj_set_style_text_color(bat, lv_color_hex(UI_PAPER), 0);
        lv_label_set_text_fmt(bat, "%d%%", soc);
        lv_obj_align(bat, LV_ALIGN_TOP_RIGHT, -10, 30);
    }
}

static lv_obj_t *body_label(lv_obj_t *parent, int x, int y, const lv_font_t *font,
                            uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_pos(label, x, y);
    return label;
}

// I1 画布直写位（避开 set_px 对索引格式的歧义）
static void i1_set(uint8_t *buf, int stride, int x, int y, bool on)
{
    uint8_t *byte = &buf[y * stride + x / 8];
    uint8_t mask = (uint8_t)(0x80 >> (x % 8));   // LVGL I1：MSB 在前
    if (on) *byte |= mask; else *byte &= (uint8_t)~mask;
}

static lv_obj_t *canvas_i1_create(lv_obj_t *parent, int x, int y, int w, int h,
                                  uint32_t c0, uint32_t c1)
{
    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_canvas_set_buffer(canvas, s_canvas_buf, w, h, LV_COLOR_FORMAT_I1);
    lv_canvas_set_palette(canvas, 0, lv_color_to_32(lv_color_hex(c0), LV_OPA_COVER));
    lv_canvas_set_palette(canvas, 1, lv_color_to_32(lv_color_hex(c1), LV_OPA_COVER));
    lv_obj_set_pos(canvas, x, y);
    return canvas;
}

// ---------------------------------------------------------------------------
// 二维码绘制（窗口变化才重绘，实现文档 7.1 节）
// ---------------------------------------------------------------------------
static void draw_qr(uint32_t ts)
{
    char pid[32];
    uint8_t secret[PASSPORT_SECRET_LEN];
    if (passport_store_get_string(PASSPORT_KEY_PID, pid, sizeof(pid)) != ESP_OK ||
        passport_store_get_secret(secret) != ESP_OK) {
        memset(secret, 0, sizeof(secret));
        return;
    }

    char code[PASSPORT_CODE_LEN + 1];
    char url[128];
    passport_qr_code(secret, pid, ts, code);
    memset(secret, 0, sizeof(secret));
    if (passport_qr_url(pid, ts, code, url, sizeof(url)) < 0) return;

    // 契约只需到版本 6（实现文档 6.2 节）；静态缓冲避免占用 LVGL 任务栈。
    static uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];
    static uint8_t tmp[qrcodegen_BUFFER_LEN_FOR_VERSION(6)];
    if (!qrcodegen_encodeText(url, tmp, qr, qrcodegen_Ecc_MEDIUM,
                              qrcodegen_VERSION_MIN, 6,
                              qrcodegen_Mask_AUTO, true)) {
        ESP_LOGE(TAG, "二维码编码失败");
        return;
    }
    int size = qrcodegen_getSize(qr);   // ECC-M 下应为 37（版本 5）
    int scale = QR_PX / size;
    int offset = (QR_PX - size * scale) / 2;

    memset(s_canvas_buf, 0, QR_STRIDE * QR_PX);   // 白底
    for (int my = 0; my < size; my++) {
        for (int mx = 0; mx < size; mx++) {
            if (!qrcodegen_getModule(qr, mx, my)) continue;
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    i1_set(s_canvas_buf, QR_STRIDE,
                           offset + mx * scale + dx, offset + my * scale + dy, true);
                }
            }
        }
    }
    lv_obj_invalidate(s_canvas);
    s_qr_window = passport_window(ts);
}

static void draw_avatar(void)
{
    char pid[32];
    get_pid(pid, sizeof(pid));
    uint8_t pattern[PASSPORT_AVATAR_SIZE][PASSPORT_AVATAR_SIZE];
    passport_avatar_pattern(pid, pattern);

    int scale = AVATAR_PX / PASSPORT_AVATAR_SIZE;   // 8
    memset(s_canvas_buf, 0, AVATAR_STRIDE * AVATAR_PX);
    for (int r = 0; r < PASSPORT_AVATAR_SIZE; r++) {
        for (int c = 0; c < PASSPORT_AVATAR_SIZE; c++) {
            if (!pattern[r][c]) continue;
            for (int dy = 0; dy < scale; dy++) {
                for (int dx = 0; dx < scale; dx++) {
                    i1_set(s_canvas_buf, AVATAR_STRIDE,
                           c * scale + dx, r * scale + dy, true);
                }
            }
        }
    }
    lv_obj_invalidate(s_canvas);
}

// ---------------------------------------------------------------------------
// MENU
// ---------------------------------------------------------------------------
static const char *const MENU_NAMES[] = {
    "我的身份", "登录二维码", "我的积分", "每日签到", "设置",
};
#define MENU_COUNT 5

static void menu_refresh(void)
{
    for (int i = 0; i < MENU_COUNT; i++) {
        lv_obj_t *card = lv_obj_get_parent(s_rows[i]);
        ui_pixel_set_selected(card, i == s_sel, true);
    }
}

static void build_menu(void)
{
    s_scr = ui_pixel_screen_create("创智Passport");
    add_status_bar(s_scr);
    for (int i = 0; i < MENU_COUNT; i++) {
        lv_obj_t *card = ui_pixel_panel_create(s_scr, 30, 62 + i * 38, 180, 32, UI_PAPER);
        s_rows[i] = lv_label_create(card);
        lv_obj_set_style_text_font(s_rows[i], FONT_CJK, 0);
        lv_obj_set_style_text_color(s_rows[i], lv_color_hex(UI_INK), 0);
        lv_label_set_text(s_rows[i], MENU_NAMES[i]);
        lv_obj_center(s_rows[i]);
    }
    s_sel = 0;
    menu_refresh();
    lv_obj_t *hint = body_label(s_scr, 172, 292, FONT_CJK, UI_INK);
    lv_label_set_text(hint, "OK进入");
}

// ---------------------------------------------------------------------------
// IDENTITY
// ---------------------------------------------------------------------------
static void build_identity(void)
{
    s_scr = ui_pixel_screen_create("我的身份");
    add_status_bar(s_scr);

    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 12, 62, 216, 210, UI_PAPER);

    s_canvas_buf = lv_malloc(AVATAR_STRIDE * AVATAR_PX);
    if (s_canvas_buf) {
        s_canvas = canvas_i1_create(panel, 12, 12, AVATAR_PX, AVATAR_PX,
                                    UI_PAPER, UI_SKY_DARK);
        draw_avatar();
    }

    char name[32] = "--";
    passport_store_get_string(PASSPORT_KEY_NAME, name, sizeof(name));
    lv_obj_t *name_label = body_label(panel, 86, 18, FONT_CJK, UI_INK);
    lv_label_set_text(name_label, name);

    char pid[32];
    get_pid(pid, sizeof(pid));
    lv_obj_t *id_label = body_label(panel, 86, 46, FONT_LATIN, UI_SKY_DARK);
    lv_label_set_text(id_label, pid);   // student_id 与 Passport ID 同值，只显示一个

    uint8_t in_lab = 0;
    passport_store_get_u8(PASSPORT_KEY_IN_LAB, &in_lab);
    s_body = body_label(panel, 12, 96, FONT_CJK, in_lab ? UI_GRASS_DARK : UI_INK);
    lv_label_set_text(s_body, in_lab ? "在馆中" : "未在馆");

    char stamp[24] = { 0 };
    bool has_stamp = passport_store_get_string(PASSPORT_KEY_BALANCE_TS,
                                               stamp, sizeof(stamp)) == ESP_OK;
    s_body2 = body_label(panel, 12, 126, FONT_CJK, UI_SKY_DARK);
    lv_label_set_text_fmt(s_body2, "%s%s", has_stamp ? "更新于 " : "离线缓存",
                          has_stamp ? stamp : "");
}

// ---------------------------------------------------------------------------
// QR
// ---------------------------------------------------------------------------
static void build_qr(void)
{
    s_scr = ui_pixel_screen_create("登录二维码");
    add_status_bar(s_scr);
    s_qr_window = UINT32_MAX;

    if (!passport_time_is_valid(now_ts())) {
        // 宁可不显示，也不显示一个必然过期的码（实现文档 7.4 节）
        lv_obj_t *panel = ui_pixel_panel_create(s_scr, 12, 90, 216, 110, UI_PAPER);
        s_body = lv_label_create(panel);
        lv_obj_set_style_text_font(s_body, FONT_CJK, 0);
        lv_obj_set_style_text_color(s_body, lv_color_hex(UI_INK), 0);
        lv_obj_set_width(s_body, 190);
        lv_obj_set_style_text_align(s_body, LV_TEXT_ALIGN_CENTER, 0);
        lv_label_set_text(s_body, "请联网校时后再使用\n（进积分页或签到页联网）");
        lv_obj_center(s_body);
        return;
    }

    s_canvas_buf = lv_malloc(QR_STRIDE * QR_PX);
    if (!s_canvas_buf) {
        ESP_LOGE(TAG, "QR 画布内存不足");
        return;
    }
    s_canvas = canvas_i1_create(s_scr, (240 - QR_PX) / 2, 62, QR_PX, QR_PX,
                                0xFFFFFF, 0x000000);
    draw_qr(now_ts());

    s_bar = lv_bar_create(s_scr);
    lv_obj_set_size(s_bar, QR_PX, 8);
    lv_obj_set_pos(s_bar, (240 - QR_PX) / 2, 62 + QR_PX + 4);
    lv_bar_set_range(s_bar, 0, PASSPORT_WINDOW_SECONDS);

    s_body = body_label(s_scr, 0, 62 + QR_PX + 16, FONT_CJK, UI_INK);
    lv_obj_set_width(s_body, 240);
    lv_obj_set_style_text_align(s_body, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_body, "出示此码给老师扫码");
    lv_screen_load(s_scr);
}

// ---------------------------------------------------------------------------
// POINTS
// ---------------------------------------------------------------------------
static void points_refresh(void)
{
    int32_t balance = 0;
    passport_store_get_i32(PASSPORT_KEY_BALANCE, &balance);
    lv_label_set_text_fmt(s_body, "%ld", (long)balance);

    char stamp[24] = { 0 };
    bool has_stamp = passport_store_get_string(PASSPORT_KEY_BALANCE_TS,
                                               stamp, sizeof(stamp)) == ESP_OK;
    if (passport_net_is_busy()) {
        lv_label_set_text(s_body2, "同步中");
    } else if (s_sync_ever_ok && has_stamp) {
        lv_label_set_text_fmt(s_body2, "更新于 %s", stamp);
    } else {
        lv_label_set_text(s_body2, "离线缓存");
    }

    passport_point_item_t items[PASSPORT_HISTORY_MAX];
    int n = passport_net_history(items);
    for (int i = 0; i < PASSPORT_HISTORY_MAX; i++) {
        if (!s_rows[i]) break;
        if (i < n) {
            // LVGL 内建 printf 对 %+ld 支持不全，符号手动拼
            lv_label_set_text_fmt(s_rows[i],
                                  items[i].delta >= 0 ? "+%ld %s" : "%ld %s",
                                  (long)items[i].delta, items[i].text);
        } else {
            lv_label_set_text(s_rows[i], "");
        }
    }
}

static void build_points(void)
{
    s_scr = ui_pixel_screen_create("我的积分");
    add_status_bar(s_scr);

    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 12, 62, 216, 100, UI_PAPER);
    s_body = body_label(panel, 0, 8, FONT_BIG, UI_INK);
    lv_obj_set_width(s_body, 190);
    lv_obj_set_style_text_align(s_body, LV_TEXT_ALIGN_CENTER, 0);
    s_body2 = body_label(panel, 0, 52, FONT_CJK, UI_SKY_DARK);
    lv_obj_set_width(s_body2, 190);
    lv_obj_set_style_text_align(s_body2, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *hist = ui_pixel_panel_create(s_scr, 12, 172, 216, 96, UI_PAPER);
    for (int i = 0; i < PASSPORT_HISTORY_MAX; i++) {
        s_rows[i] = body_label(hist, 8, 4 + i * 28, FONT_CJK, UI_INK);
    }
    points_refresh();
}

// ---------------------------------------------------------------------------
// CHECKIN
// ---------------------------------------------------------------------------
static void checkin_set_status(const char *text)
{
    if (s_current == PASSPORT_SCREEN_CHECKIN && s_body) {
        lv_label_set_text(s_body, text);
    }
}

static void build_checkin(void)
{
    s_scr = ui_pixel_screen_create("每日签到");
    add_status_bar(s_scr);

    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 12, 100, 216, 100, UI_PAPER);
    s_body = lv_label_create(panel);
    lv_obj_set_style_text_font(s_body, FONT_CJK, 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(UI_INK), 0);
    lv_obj_set_width(s_body, 190);
    lv_obj_set_style_text_align(s_body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_body);

    // 本地日期短路：当日已签直接显示结果（实现文档 7.3 节）
    char today[64];
    {
        time_t now = time(NULL);
        struct tm tm_now;
        localtime_r(&now, &tm_now);
        snprintf(today, sizeof(today), "%04d-%02d-%02d",
                 tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
    }
    char last[11] = { 0 };
    passport_store_get_string(PASSPORT_KEY_LAST_CHECKIN, last, sizeof(last));
    if (strcmp(last, today) == 0) {
        lv_label_set_text(s_body, "今日已签");
    } else {
        lv_label_set_text(s_body, "连网中");
        passport_net_request(PASSPORT_NET_REQ_CHECKIN);
    }
}

// ---------------------------------------------------------------------------
// SETTINGS
// ---------------------------------------------------------------------------
static void settings_refresh(void)
{
    char line[48];
    char stamp[24] = { 0 };
    bool has_stamp = passport_store_get_string(PASSPORT_KEY_BALANCE_TS,
                                               stamp, sizeof(stamp)) == ESP_OK;
    snprintf(line, sizeof(line), "上次同步 %s", has_stamp ? stamp : "无");
    lv_label_set_text(s_rows[0], line);

    lv_label_set_text(s_rows[1], passport_time_is_valid(now_ts())
                                 ? "校时 已校时" : "校时 未校时");

    char pid[32];
    get_pid(pid, sizeof(pid));
    snprintf(line, sizeof(line), "设备 %s", pid);
    lv_label_set_text(s_rows[2], line);

    const esp_app_desc_t *desc = esp_app_get_description();
    snprintf(line, sizeof(line), "版本 %s", desc->version);
    lv_label_set_text(s_rows[3], line);

    lv_label_set_text(s_rows[4], s_factory_armed ? "再按OK确认" : "恢复出厂");

    for (int i = 0; i < 5; i++) {
        lv_obj_t *card = lv_obj_get_parent(s_rows[i]);
        ui_pixel_set_selected(card, i == s_sel, true);
    }
}

static void build_settings(void)
{
    s_scr = ui_pixel_screen_create("设置");
    add_status_bar(s_scr);
    for (int i = 0; i < 5; i++) {
        lv_obj_t *card = ui_pixel_panel_create(s_scr, 20, 62 + i * 40, 200, 34, UI_PAPER);
        s_rows[i] = lv_label_create(card);
        lv_obj_set_style_text_font(s_rows[i], FONT_CJK, 0);
        lv_obj_set_style_text_color(s_rows[i], lv_color_hex(UI_INK), 0);
        lv_obj_center(s_rows[i]);
    }
    s_sel = 0;
    s_factory_armed = false;
    settings_refresh();
}

// 5 秒长按检测：LONG 事件（约 1.5s）时再计时 3.5s，仍按住 OK 即满足
static void hold_check_cb(lv_timer_t *timer)
{
    (void)timer;
    s_hold_timer = NULL;
    int mv = bsp_button_read_mv();
    if (mv >= 447 && mv <= 1900) {   // OK 键电压窗口（bsp_pins.h BSP_BTN_MV_TABLE）
        if (s_ui_queue) {
            // 经 UI 队列通知 app 进 demo 菜单，避免跨上下文调用
            passport_ui_evt_t evt = { .type = 0, .arg1 = PASSPORT_ACTION_ENTER_DEMO };
            xQueueSend(s_ui_queue, &evt, 0);
        }
    }
}

// ---------------------------------------------------------------------------
// 屏幕装配/拆解
// ---------------------------------------------------------------------------
typedef void (*build_fn_t)(void);
static const build_fn_t BUILDERS[PASSPORT_SCREEN_COUNT] = {
    build_menu, build_identity, build_qr,
    build_points, build_checkin, build_settings,
};

static void teardown(void)
{
    if (s_hold_timer) {
        lv_timer_delete(s_hold_timer);
        s_hold_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    lv_free(s_canvas_buf);
    s_canvas_buf = NULL;
    s_canvas = NULL;
    s_status = s_body = s_body2 = s_bar = NULL;
    memset(s_rows, 0, sizeof(s_rows));
    s_factory_armed = false;
}

// ---------------------------------------------------------------------------
// 公开 API
// ---------------------------------------------------------------------------
void passport_ui_init(QueueHandle_t ui_queue)
{
    // 结构体复制保持原字体的位图/字距数据，仅补 fallback 指针。
    s_font_cjk = font_sm_cjk_16;
    s_font_cjk.fallback = &lv_font_source_han_sans_sc_16_cjk;
    s_ui_queue = ui_queue;
}

void passport_ui_show(passport_screen_t screen)
{
    teardown();
    s_current = screen;
    BUILDERS[screen]();
    if (s_scr) lv_screen_load(s_scr);
}

passport_screen_t passport_ui_current(void)
{
    return s_current;
}

passport_action_t passport_ui_handle_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    switch (s_current) {
    case PASSPORT_SCREEN_MENU:
        if (ev == BSP_BTN_CLICK) {
            if (btn == BSP_BTN_UP) {
                s_sel = (s_sel + MENU_COUNT - 1) % MENU_COUNT;
                menu_refresh();
            } else if (btn == BSP_BTN_DOWN) {
                s_sel = (s_sel + 1) % MENU_COUNT;
                menu_refresh();
            } else if (btn == BSP_BTN_OK) {
                passport_ui_show((passport_screen_t)(PASSPORT_SCREEN_IDENTITY + s_sel));
            }
        }
        break;

    case PASSPORT_SCREEN_QR:
        if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK && s_canvas) {
            draw_qr(now_ts());   // OK 立即重绘
        }
        break;

    case PASSPORT_SCREEN_POINTS:
        if (btn == BSP_BTN_OK && ev == BSP_BTN_CLICK && !passport_net_is_busy()) {
            lv_label_set_text(s_body2, "同步中");
            passport_net_request(PASSPORT_NET_REQ_SYNC);
        }
        break;

    case PASSPORT_SCREEN_CHECKIN:
        break;   // 流程页：进入即签到，无额外按键动作

    case PASSPORT_SCREEN_SETTINGS:
        if (ev == BSP_BTN_CLICK) {
            if (btn == BSP_BTN_UP) {
                s_sel = (s_sel + 4) % 5;
                settings_refresh();
            } else if (btn == BSP_BTN_DOWN) {
                s_sel = (s_sel + 1) % 5;
                settings_refresh();
            } else if (btn == BSP_BTN_OK && s_sel == 4) {
                if (s_factory_armed) {
                    passport_store_erase_all();
                    esp_restart();
                }
                s_factory_armed = true;
                settings_refresh();
            }
        } else if (btn == BSP_BTN_OK && ev == BSP_BTN_LONG && !s_hold_timer) {
            // 隐藏入口候选：LONG 后再计时 3.5s（合计约 5 秒）
            s_hold_timer = lv_timer_create(hold_check_cb, DEMO_HOLD_EXTRA_MS, NULL);
            lv_timer_set_repeat_count(s_hold_timer, 1);
        }
        break;

    default:
        break;
    }
    return PASSPORT_ACTION_NONE;
}

void passport_ui_on_net_event(const passport_ui_evt_t *evt)
{
    switch (evt->type) {
    case PASSPORT_UI_EVT_SYNC_OK:
        s_sync_ever_ok = true;
        if (s_current == PASSPORT_SCREEN_POINTS) points_refresh();
        if (s_current == PASSPORT_SCREEN_IDENTITY) passport_ui_show(PASSPORT_SCREEN_IDENTITY);
        break;
    case PASSPORT_UI_EVT_SYNC_FAIL:
        if (s_current == PASSPORT_SCREEN_POINTS) {
            lv_label_set_text(s_body2, "同步失败");
        }
        break;
    case PASSPORT_UI_EVT_CHECKIN_OK:
        s_sync_ever_ok = true;
        checkin_set_status("签到成功");
        break;
    case PASSPORT_UI_EVT_CHECKIN_DUP:
        checkin_set_status("今日已签");
        break;
    case PASSPORT_UI_EVT_CHECKIN_FAIL:
        checkin_set_status("网络失败请重试");
        break;
    case PASSPORT_UI_EVT_TIME_VALID:
        if (s_current == PASSPORT_SCREEN_QR && !s_canvas) {
            passport_ui_show(PASSPORT_SCREEN_QR);   // 校时完成，重建可用的二维码页
        }
        break;
    case PASSPORT_UI_EVT_HEARTBEAT_OK:
        s_sync_ever_ok = true;
        if (s_current == PASSPORT_SCREEN_POINTS) points_refresh();
        break;
    case PASSPORT_UI_EVT_NET_IDLE:
        if (s_current == PASSPORT_SCREEN_POINTS) points_refresh();
        break;
    default:
        break;
    }
}

void passport_ui_tick(void)
{
    if (s_current == PASSPORT_SCREEN_QR && s_canvas) {
        uint32_t ts = now_ts();
        if (!passport_time_is_valid(ts)) return;
        uint32_t window = passport_window(ts);
        if (window != s_qr_window) draw_qr(ts);   // 30 秒轮换
        if (s_bar) {
            lv_bar_set_value(s_bar,
                             (int32_t)(PASSPORT_WINDOW_SECONDS - ts % PASSPORT_WINDOW_SECONDS),
                             LV_ANIM_OFF);
        }
    }
}

void passport_ui_clear_sensitive(void)
{
    if (s_current == PASSPORT_SCREEN_QR && s_canvas && s_canvas_buf) {
        memset(s_canvas_buf, 0, QR_STRIDE * QR_PX);   // 清码防断背光残影
        lv_obj_invalidate(s_canvas);
    }
}
