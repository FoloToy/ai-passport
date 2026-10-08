// main/finder_view.c —— 见 finder_view.h。
//
// 界面是自建的：不用 ui_pixel_*，不用基线 demo 的菜单/测试屏/视觉外壳。
// 布局按 240x320 手工排布；配色为近黑底 + 纯白字（对比度 >= 4.5:1）。
#include "finder_view.h"

#include "bsp_battery.h"

#include "lvgl.h"

#include <stdio.h>
#include <string.h>

// ---- 配色（近黑底 + 纯白字 + 少量强调色）--------------------------------
#define COL_BG      0x0A0A0A
#define COL_TEXT    0xFFFFFF
#define COL_DIM     0x555555
#define COL_RING    0x2A2A2A
#define COL_SELECT  0x00D0FF
#define COL_BAR_ON  0x00E060
#define COL_BAR_OFF 0x1E2A20
#define COL_WARN    0xFFC000

// ---- 雷达几何 -----------------------------------------------------------
// 中心与半径：环形只编码"远近"（半径），角度由地址哈希固定且【不承载任何信息】。
#define RADAR_CX      120
#define RADAR_CY      112
#define DOT_SIZE       12
#define DOT_SEL_SIZE   18
#define RADAR_SLOTS    12 // 30 度一格；取整到固定格位，避免逐帧浮点三角函数

// 每 30 度的单位向量，放大 1000 倍；只做整数乘除，无浮点。
static const int16_t k_slot_cos[RADAR_SLOTS] = {
    1000, 866, 500, 0, -500, -866, -1000, -866, -500, 0, 500, 866,
};
static const int16_t k_slot_sin[RADAR_SLOTS] = {
    0, 500, 866, 1000, 866, 500, 0, -500, -866, -1000, -866, -500,
};

// 等级 -> 半径：等级越高越靠近中心。等级 0（无信号）贴在最外圈并置灰。
static int16_t radius_for_level(int8_t level)
{
    if (level < 0) {
        level = 0;
    }
    if (level > 5) {
        level = 5;
    }
    return (int16_t)(20 + (5 - level) * 16);
}

static void style_screen(lv_obj_t *scr)
{
    lv_obj_set_style_bg_color(scr, lv_color_hex(COL_BG), 0);
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
}

static lv_obj_t *add_text(lv_obj_t *parent, const lv_font_t *font, uint32_t color,
                          lv_align_t align, int x, int y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(label, align, x, y);
    return label;
}

static lv_obj_t *add_block(lv_obj_t *parent, uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_radius(obj, 3, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

// ---- 页面对象 -----------------------------------------------------------
static lv_obj_t  *s_scr;
static finder_page_t s_page;
static bool        s_active;

static lv_obj_t *s_battery; // 右上角电量（三页共用）
static lv_obj_t *s_notice;  // 瞬时提示

// 雷达页
static lv_obj_t *s_ring[RADAR_SLOTS]; // 仅作背景刻度环，不表示方位
static lv_obj_t *s_dot[FINDER_TABLE_MAX];
static lv_obj_t *s_dot_sel;           // 选中高亮环（非方位标识）
static lv_obj_t *s_sel_name;
static lv_obj_t *s_sel_seg[5];
static lv_obj_t *s_hint;

// 聚焦页
static lv_obj_t *s_status;
static lv_obj_t *s_bar[5];
static lv_obj_t *s_trend;
static lv_obj_t *s_focus_name;
static lv_obj_t *s_mute;

// ---- 构建 ---------------------------------------------------------------

static void build_header(lv_obj_t *scr, const char *title)
{
    lv_obj_t *t = add_text(scr, &lv_font_montserrat_14, COL_TEXT, LV_ALIGN_TOP_LEFT, 10, 6);
    lv_label_set_text(t, title);

    s_battery = add_text(scr, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_TOP_RIGHT, -10, 6);
    lv_label_set_text(s_battery, "");
}

static void build_intro(lv_obj_t *scr)
{
    build_header(scr, "POCKET FINDER");

    lv_obj_t *body = add_text(scr, &lv_font_montserrat_14, COL_TEXT, LV_ALIGN_TOP_LEFT, 18, 60);
    lv_obj_set_width(body, 204);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_LEFT, 0);
    lv_label_set_text(body,
                      "This finds nearby\n"
                      "Bluetooth devices.\n"
                      "\n"
                      "The ring shows how strong\n"
                      "each one is.\n"
                      "\n"
                      "The angle of each dot is\n"
                      "random. It does NOT point\n"
                      "toward anything. Only\n"
                      "closer / farther is real.");

    lv_obj_t *cta = add_text(scr, &lv_font_montserrat_20, COL_SELECT, LV_ALIGN_BOTTOM_MID, 0, -40);
    lv_label_set_text(cta, "OK");
    lv_obj_t *cta2 = add_text(scr, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_label_set_text(cta2, "to start");
}

static void build_radar(lv_obj_t *scr)
{
    build_header(scr, "RADAR");

    // 同心刻度环：只表达半径档位，方向中性。
    for (int i = 0; i < RADAR_SLOTS; i++) {
        int16_t r = (int16_t)(20 + i * 16);
        s_ring[i] = lv_obj_create(scr);
        lv_obj_set_size(s_ring[i], (int)r * 2 + 2, (int)r * 2 + 2);
        lv_obj_set_style_bg_opa(s_ring[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_ring[i], 1, 0);
        lv_obj_set_style_border_color(s_ring[i], lv_color_hex(COL_RING), 0);
        lv_obj_set_style_radius(s_ring[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_align(s_ring[i], LV_ALIGN_TOP_LEFT, RADAR_CX - (int)r - 1, RADAR_CY - (int)r - 1);
        lv_obj_remove_flag(s_ring[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    // 选中高亮环：在设备点【之下】绘制，用环形高亮而不是方位来描述"选中的是哪个"。
    s_dot_sel = lv_obj_create(scr);
    lv_obj_set_size(s_dot_sel, DOT_SEL_SIZE, DOT_SEL_SIZE);
    lv_obj_set_style_bg_opa(s_dot_sel, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_dot_sel, 2, 0);
    lv_obj_set_style_border_color(s_dot_sel, lv_color_hex(COL_SELECT), 0);
    lv_obj_set_style_radius(s_dot_sel, LV_RADIUS_CIRCLE, 0);
    lv_obj_add_flag(s_dot_sel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_remove_flag(s_dot_sel, LV_OBJ_FLAG_SCROLLABLE);

    for (int i = 0; i < FINDER_TABLE_MAX; i++) {
        s_dot[i] = lv_obj_create(scr);
        lv_obj_set_size(s_dot[i], DOT_SIZE, DOT_SIZE);
        lv_obj_set_style_bg_color(s_dot[i], lv_color_hex(COL_TEXT), 0);
        lv_obj_set_style_bg_opa(s_dot[i], LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(s_dot[i], 0, 0);
        lv_obj_set_style_radius(s_dot[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_add_flag(s_dot[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(s_dot[i], LV_OBJ_FLAG_SCROLLABLE);
    }

    // 名称可能很长（最多 24 字符），给固定宽度让它换行而不是溢出屏幕。
    s_sel_name = add_text(scr, &lv_font_montserrat_14, COL_TEXT, LV_ALIGN_TOP_MID, 0, 198);
    lv_obj_set_width(s_sel_name, 224);
    lv_label_set_text(s_sel_name, "NO DEVICES NEARBY");

    for (int i = 0; i < 5; i++) {
        s_sel_seg[i] = add_block(scr, COL_BAR_OFF);
        lv_obj_set_size(s_sel_seg[i], 36, 12);
        lv_obj_align(s_sel_seg[i], LV_ALIGN_TOP_LEFT,
                     120 - (5 * 36 + 4 * 4) / 2 + i * (36 + 4), 234);
    }

    s_notice = add_text(scr, &lv_font_montserrat_14, COL_WARN, LV_ALIGN_TOP_MID, 0, 250);
    lv_label_set_text(s_notice, "");

    // 常驻免责提示（PRD §9.2 第 6 条）：必须常驻，不只是首次显示。
    // 折成两行：整句在 Montserrat 14 下约 243px，超过 240px 屏宽会被截断。
    s_hint = add_text(scr, &lv_font_montserrat_14, COL_DIM, LV_ALIGN_BOTTOM_MID, 0, -6);
    lv_label_set_text(s_hint, "ANGLE IS RANDOM\nCLOSER/FARTHER IS REAL");
}

static void build_focus(lv_obj_t *scr)
{
    build_header(scr, "SIGNAL");

    // 状态文案会给固定宽度并允许换行："WEAK - TRY ANOTHER DIRECTION" 单行约 260px，
    // 超过 240px 屏宽。折行后最多占两行（28..62），五级条最高柱从 y=74 起，不会压上。
    s_status = add_text(scr, &lv_font_montserrat_14, COL_TEXT, LV_ALIGN_TOP_MID, 0, 28);
    lv_obj_set_width(s_status, 224);
    lv_label_set_text(s_status, "");

    // 五级条是唯一主元素：高度为主编码，颜色为辅（色盲与暗光下仍可读）。
    static const int bar_h[5] = { 34, 68, 102, 136, 170 };
    const int bottom = 244;
    for (int i = 0; i < 5; i++) {
        s_bar[i] = add_block(scr, COL_BAR_OFF);
        lv_obj_set_size(s_bar[i], 32, bar_h[i]);
        lv_obj_align(s_bar[i], LV_ALIGN_TOP_LEFT, 16 + i * (32 + 12), bottom - bar_h[i]);
    }

    s_trend = add_text(scr, &lv_font_montserrat_14, COL_TEXT, LV_ALIGN_TOP_MID, 0, 248);
    lv_label_set_text(s_trend, "STEADY");

    s_focus_name = add_text(scr, &lv_font_montserrat_14, COL_TEXT, LV_ALIGN_TOP_MID, 0, 266);
    lv_label_set_text(s_focus_name, "");

    s_mute = add_text(scr, &lv_font_montserrat_14, COL_WARN, LV_ALIGN_TOP_MID, 0, 284);
    lv_label_set_text(s_mute, "OK: NEXT");

    s_notice = add_text(scr, &lv_font_montserrat_14, COL_WARN, LV_ALIGN_TOP_MID, 0, 302);
    lv_label_set_text(s_notice, "");
}

// ---- 生命周期 -----------------------------------------------------------

void finder_view_show(finder_page_t page)
{
    finder_view_delete();

    s_scr = lv_obj_create(NULL);
    style_screen(s_scr);

    s_page = page;
    switch (page) {
    case FINDER_PAGE_INTRO:
        build_intro(s_scr);
        break;
    case FINDER_PAGE_RADAR:
        build_radar(s_scr);
        break;
    case FINDER_PAGE_FOCUS:
        build_focus(s_scr);
        break;
    default:
        break;
    }

    s_active = true;
    lv_screen_load(s_scr);
}

void finder_view_delete(void)
{
    if (!s_scr) {
        s_active = false;
        return;
    }
    lv_obj_delete(s_scr);
    s_scr = NULL;
    s_active = false;

    s_battery = NULL;
    s_notice = NULL;
    s_dot_sel = NULL;
    s_sel_name = NULL;
    s_hint = NULL;
    s_status = NULL;
    s_trend = NULL;
    s_focus_name = NULL;
    s_mute = NULL;
    for (int i = 0; i < FINDER_TABLE_MAX; i++) {
        s_dot[i] = NULL;
    }
    for (int i = 0; i < 5; i++) {
        s_sel_seg[i] = NULL;
        s_bar[i] = NULL;
    }
    for (int i = 0; i < RADAR_SLOTS; i++) {
        s_ring[i] = NULL;
    }
}

bool finder_view_active(void)
{
    return s_active;
}

// ---- 刷新 ---------------------------------------------------------------

static void update_battery(void)
{
    if (!s_battery) {
        return;
    }
    int soc = bsp_battery_soc();
    if (soc < 0) {
        lv_label_set_text(s_battery, ""); // 读数不可用时优雅降级，不画数字
        return;
    }
    lv_label_set_text_fmt(s_battery, "%d%%", soc);
}

static void update_notice(const char *notice)
{
    if (!s_notice) {
        return;
    }
    lv_label_set_text(s_notice, (notice && notice[0] != '\0') ? notice : "");
}

static void render_radar(const finder_ui_t *ui, const finder_table_t *table, int64_t now_ms)
{
    uint8_t shown = 0;
    for (int i = 0; i < FINDER_TABLE_MAX; i++) {
        const finder_entry_t *e = &table->entry[i];
        if (!e->used) {
            if (s_dot[i]) {
                lv_obj_add_flag(s_dot[i], LV_OBJ_FLAG_HIDDEN);
            }
            continue;
        }

        int8_t level = finder_rssi_level(&e->rssi, now_ms);
        int16_t r = radius_for_level(level);
        uint8_t slot = (uint8_t)(((e->angle_deg + 15u) / 30u) % RADAR_SLOTS);
        int x = RADAR_CX + (k_slot_cos[slot] * r) / 1000;
        int y = RADAR_CY + (k_slot_sin[slot] * r) / 1000;

        lv_obj_remove_flag(s_dot[i], LV_OBJ_FLAG_HIDDEN);
        lv_obj_align(s_dot[i], LV_ALIGN_TOP_LEFT, x - DOT_SIZE / 2, y - DOT_SIZE / 2);

        // 无信号（等级 0）用暗色表示"还在表里但听不到"，不要伪装成强信号。
        lv_obj_set_style_bg_color(s_dot[i],
                                  lv_color_hex(level > 0 ? COL_TEXT : COL_DIM), 0);
        shown++;
    }

    const finder_entry_t *sel = NULL;
    if (ui->selected >= 0 && ui->selected < FINDER_TABLE_MAX &&
        table->entry[ui->selected].used) {
        sel = &table->entry[ui->selected];
    }

    if (s_dot_sel) {
        if (sel && s_dot[ui->selected]) {
            lv_obj_remove_flag(s_dot_sel, LV_OBJ_FLAG_HIDDEN);
            lv_coord_t dx = lv_obj_get_x(s_dot[ui->selected]);
            lv_coord_t dy = lv_obj_get_y(s_dot[ui->selected]);
            lv_obj_align(s_dot_sel, LV_ALIGN_TOP_LEFT,
                         dx - (DOT_SEL_SIZE - DOT_SIZE) / 2,
                         dy - (DOT_SEL_SIZE - DOT_SIZE) / 2);
        } else {
            lv_obj_add_flag(s_dot_sel, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (s_sel_name) {
        if (sel) {
            if (sel->name[0] != '\0') {
                lv_label_set_text(s_sel_name, sel->name);
            } else {
                lv_label_set_text_fmt(s_sel_name, "DEVICE %02X%02X",
                                      sel->addr[4], sel->addr[5]);
            }
        } else if (shown == 0) {
            lv_label_set_text(s_sel_name, "NO DEVICES NEARBY");
        } else {
            lv_label_set_text(s_sel_name, "SELECT A DEVICE");
        }
    }

    int8_t sel_level = sel ? finder_rssi_level(&sel->rssi, now_ms) : 0;
    for (int i = 0; i < 5; i++) {
        if (s_sel_seg[i]) {
            lv_obj_set_style_bg_color(s_sel_seg[i],
                                      lv_color_hex(i < sel_level ? COL_BAR_ON : COL_BAR_OFF), 0);
        }
    }
}

static void render_focus(const finder_ui_t *ui, const finder_table_t *table, int64_t now_ms)
{
    int8_t level = 0;
    const finder_entry_t *e = NULL;
    if (ui->focus >= 0 && ui->focus < FINDER_TABLE_MAX && table->entry[ui->focus].used) {
        e = &table->entry[ui->focus];
        level = finder_rssi_level(&e->rssi, now_ms);
    }

    for (int i = 0; i < 5; i++) {
        if (s_bar[i]) {
            lv_obj_set_style_bg_color(s_bar[i],
                                      lv_color_hex(i < level ? COL_BAR_ON : COL_BAR_OFF), 0);
        }
    }

    if (s_status) {
        // 文案规则（PRD §7）：只表达相对强弱，绝不给距离或方位承诺。
        const char *text = "NO SIGNAL";
        if (e == NULL) {
            text = "DEVICE GONE";
        } else if (level == 0) {
            text = "NO SIGNAL";
        } else if (level <= 2) {
            text = "WEAK - TRY ANOTHER DIRECTION";
        } else if (level <= 4) {
            text = "GETTING STRONGER";
        } else {
            text = "VERY CLOSE - CHECK HERE";
        }
        lv_label_set_text(s_status, text);
    }

    if (s_trend) {
        finder_trend_t trend = e ? finder_rssi_trend(&e->rssi) : FINDER_TREND_STEADY;
        const char *text = "STEADY";
        if (e && level > 0) {
            if (trend == FINDER_TREND_STRONGER) {
                text = "STRONGER";
            } else if (trend == FINDER_TREND_WEAKER) {
                text = "WEAKER";
            }
        }
        lv_label_set_text(s_trend, text);
    }

    if (s_focus_name) {
        if (e) {
            if (e->name[0] != '\0') {
                lv_label_set_text(s_focus_name, e->name);
            } else {
                lv_label_set_text_fmt(s_focus_name, "DEVICE %02X%02X", e->addr[4], e->addr[5]);
            }
        } else {
            lv_label_set_text(s_focus_name, "-");
        }
    }

    if (s_mute) {
        if (ui->muted) {
            lv_label_set_text(s_mute, "MUTED");
            lv_obj_set_style_text_color(s_mute, lv_color_hex(COL_WARN), 0);
        } else if (ui->show_diag && e) {
            lv_label_set_text_fmt(s_mute, "%d dBm", (int)finder_rssi_ema(&e->rssi));
            lv_obj_set_style_text_color(s_mute, lv_color_hex(COL_DIM), 0);
        } else {
            lv_label_set_text(s_mute, "OK: NEXT");
            lv_obj_set_style_text_color(s_mute, lv_color_hex(COL_DIM), 0);
        }
    }
}

void finder_view_update(const finder_ui_t *ui, const finder_table_t *table,
                        int64_t now_ms, const char *notice)
{
    if (!s_active || !ui || !table || !s_scr) {
        return;
    }

    update_battery();
    update_notice(notice);

    if (s_page == FINDER_PAGE_RADAR) {
        render_radar(ui, table, now_ms);
    } else if (s_page == FINDER_PAGE_FOCUS) {
        render_focus(ui, table, now_ms);
    }
}
