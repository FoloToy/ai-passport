#include "ui_safety.h"

#include "ui_pixel.h"

#include "lvgl.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

LV_FONT_DECLARE(lv_font_cn_16);

static lv_obj_t *s_screen;

static size_t utf8_sequence_length(uint8_t lead)
{
    if (lead < 0x80) return 1;
    if ((lead & 0xE0) == 0xC0) return 2;
    if ((lead & 0xF0) == 0xE0) return 3;
    if ((lead & 0xF8) == 0xF0) return 4;
    return 0;
}

static uint32_t utf8_codepoint(const uint8_t *bytes, size_t count)
{
    if (count == 1) return bytes[0];
    uint32_t value = bytes[0] & ((1u << (7u - count)) - 1u);
    for (size_t i = 1; i < count; ++i) {
        value = (value << 6) | (bytes[i] & 0x3Fu);
    }
    return value;
}

static void display_safe_text(const char *input, const lv_font_t *font,
                              char *output, size_t capacity)
{
    size_t in = 0;
    size_t out = 0;
    if (capacity == 0) return;
    while (input && input[in] != '\0' && out + 1 < capacity) {
        const uint8_t *bytes = (const uint8_t *)input + in;
        size_t count = utf8_sequence_length(bytes[0]);
        bool valid = count > 0;
        for (size_t i = 1; valid && i < count; ++i) {
            valid = (bytes[i] & 0xC0) == 0x80;
        }
        if (!valid) {
            output[out++] = '?';
            ++in;
            continue;
        }
        uint32_t codepoint = utf8_codepoint(bytes, count);
        lv_font_glyph_dsc_t glyph;
        bool supported = codepoint == '\n' || codepoint == '\r' ||
                         lv_font_get_glyph_dsc(font, &glyph, codepoint, 0);
        if (!supported) {
            output[out++] = '?';
        } else if (out + count < capacity) {
            memcpy(output + out, bytes, count);
            out += count;
        } else {
            break;
        }
        in += count;
    }
    output[out] = '\0';
}

static lv_obj_t *new_screen(const char *title)
{
    lv_obj_t *old = s_screen;
    s_screen = ui_pixel_screen_create(title);
    lv_screen_load(s_screen);
    if (old) lv_obj_delete_async(old);
    return s_screen;
}

static lv_obj_t *safe_label(lv_obj_t *parent, const char *text, int width,
                            uint32_t color)
{
    char safe[640];
    display_safe_text(text ? text : "", &lv_font_cn_16, safe, sizeof(safe));
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, safe);
    lv_obj_set_width(label, width);
    lv_label_set_long_mode(label, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(label, &lv_font_cn_16, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_line_space(label, 5, 0);
    return label;
}

static void add_status(lv_obj_t *screen, int page, int battery_percent)
{
    if (battery_percent >= 0) {
        lv_obj_t *battery = lv_label_create(screen);
        lv_label_set_text_fmt(battery, "%d%%", battery_percent);
        lv_obj_set_style_text_font(battery, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(battery, lv_color_hex(UI_INK), 0);
        lv_obj_set_pos(battery, 178, 30);
    }
    if (page >= 0) {
        lv_obj_t *indicator = lv_label_create(screen);
        lv_label_set_text_fmt(indicator, "%d/%d", page + 1,
                              UI_SAFETY_PAGE_COUNT);
        lv_obj_set_style_text_font(indicator, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(indicator, lv_color_hex(UI_INK), 0);
        lv_obj_set_pos(indicator, 190, 47);
    }
}

static lv_obj_t *content_panel(lv_obj_t *screen)
{
    return ui_pixel_panel_create(screen, 10, 64, 220, 214, UI_PAPER);
}

static void add_footer(lv_obj_t *screen, const char *text)
{
    lv_obj_t *footer = safe_label(screen, text, 232, UI_INK);
    lv_obj_set_style_text_align(footer, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(footer, 4, 292);
}

static void center_body(lv_obj_t *panel, const char *text)
{
    lv_obj_t *body = safe_label(panel, text, 188, UI_INK);
    lv_obj_set_style_text_align(body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(body);
}

void ui_safety_show_profile(const safety_profile_t *profile, int page,
                            bool has_wechat_qr, int battery_percent)
{
    if (!profile) return;
    if (page < 0) page = 0;
    if (page >= UI_SAFETY_PAGE_COUNT) page = UI_SAFETY_PAGE_COUNT - 1;

    lv_obj_t *screen = new_screen("SAFE CARD");
    add_status(screen, page, battery_percent);
    lv_obj_t *panel = content_panel(screen);
    char body[560];

    if (page == 0) {
        snprintf(body, sizeof(body), "%s\n\n%s",
                 profile->name[0] ? profile->name : "请帮助我",
                 profile->help_text[0] ? profile->help_text :
                 "您好，我可能迷路了，请帮我联系家人");
    } else if (page == 1) {
        char phone[40];
        safety_profile_mask_phone(profile, phone, sizeof(phone));
        snprintf(body, sizeof(body), "联系家人\n\n%s%s%s\n\n%s%s%s",
                 profile->contact_name[0] ? profile->contact_name : "家属",
                 profile->relation[0] ? " / " : "", profile->relation,
                 phone[0] ? phone : "未填写电话",
                 profile->backup_phone[0] ? "\n备用: " : "",
                 profile->backup_phone);
    } else if (page == 2) {
        const char *address = profile->show_full_address &&
                              profile->home_address[0]
                                  ? profile->home_address
                                  : profile->home_area;
        snprintf(body, sizeof(body), "帮我回家\n\n%s",
                 address[0] ? address : "未填写居住信息");
    } else if (page == 3) {
        snprintf(body, sizeof(body), "健康提醒\n\n%s",
                 profile->medical[0] ? profile->medical :
                 "暂无特别健康提醒");
    } else {
        snprintf(body, sizeof(body), "微信联系\n\n%s\n\n%s",
                 has_wechat_qr ? "按确认键显示家属微信二维码" :
                                 "尚未上传微信二维码",
                 profile->wechat_note[0] ? profile->wechat_note : "");
    }
    center_body(panel, body);
    add_footer(screen, page == UI_SAFETY_PAGE_COUNT - 1
                           ? "上下键翻页 | 确认显示二维码"
                           : "上下键翻页 | 长按确认重新设置");
}

void ui_safety_show_setup(const char *ssid, const char *password,
                          bool first_setup, int battery_percent)
{
    (void)first_setup;
    lv_obj_t *screen = new_screen("LOCAL SETUP");
    add_status(screen, -1, battery_percent);
    char payload[160];
    snprintf(payload, sizeof(payload), "WIFI:T:WPA;S:%s;P:%s;;",
             ssid, password);
    lv_obj_t *qr = lv_qrcode_create(screen);
    lv_qrcode_set_size(qr, 178);
    lv_qrcode_set_dark_color(qr, lv_color_black());
    lv_qrcode_set_light_color(qr, lv_color_white());
    lv_qrcode_set_quiet_zone(qr, true);
    lv_qrcode_set_data(qr, payload);
    lv_obj_set_pos(qr, 31, 57);

    char hint[120];
    snprintf(hint, sizeof(hint), "相机扫码连接热点\n%s\n打开 192.168.4.1", ssid);
    lv_obj_t *label = safe_label(screen, hint, 224, UI_INK);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(label, 8, 241);
}

void ui_safety_show_reset_confirm(int battery_percent)
{
    lv_obj_t *screen = new_screen("RESET?");
    add_status(screen, -1, battery_percent);
    lv_obj_t *panel = content_panel(screen);
    center_body(panel, "重新设置安心牌?\n\n将临时开启设备热点\n旧资料会保留到新资料保存成功");
    add_footer(screen, "确认继续 | 上下键取消");
}

void ui_safety_show_saved(int battery_percent)
{
    lv_obj_t *screen = new_screen("SAVED");
    add_status(screen, -1, battery_percent);
    lv_obj_t *panel = content_panel(screen);
    center_body(panel, "设置完成\n\n资料已保存在设备本地\n无线网络已经关闭");
    add_footer(screen, "即将返回安心牌");
}

void ui_safety_show_qr_loading(int battery_percent)
{
    lv_obj_t *screen = new_screen("WECHAT QR");
    add_status(screen, -1, battery_percent);
    lv_obj_t *panel = content_panel(screen);
    center_body(panel, "正在读取家属二维码...");
    add_footer(screen, "请稍候");
}

void ui_safety_show_qr_error(int battery_percent)
{
    lv_obj_t *screen = new_screen("QR ERROR");
    add_status(screen, -1, battery_percent);
    lv_obj_t *panel = content_panel(screen);
    center_body(panel, "二维码读取失败\n\n请长按确认键重新设置\n并再次上传清晰的二维码截图");
    add_footer(screen, "上下键返回资料页");
}
