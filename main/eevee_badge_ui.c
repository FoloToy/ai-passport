#include "eevee_badge_ui.h"
#include "eevee_font.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>

#define SCREEN_W 240
#define SCREEN_H 320

// 配色体系 (现代商务工牌风)
#define COLOR_BG            0xF1F5F9
#define COLOR_CARD          0xFFFFFF
#define COLOR_BORDER        0xCBD5E1
#define COLOR_TEXT_MAIN     0x0F172A
#define COLOR_TEXT_MUTED    0x64748B
#define COLOR_TOPBAR        0x0F172A
#define COLOR_PRIMARY       0x2563EB
#define COLOR_SUCCESS       0x10B981
#define COLOR_WARNING       0xF59E0B
#define COLOR_DANGER        0xEF4444
#define COLOR_TEAL          0x0F766E

static lv_obj_t *s_main_scr;
static lv_obj_t *s_topbar;
static lv_obj_t *s_lbl_wifi;
static lv_obj_t *s_lbl_page_indicator;
static lv_obj_t *s_lbl_battery;

static lv_obj_t *s_pages[EEVEE_PAGE_COUNT];
static eevee_ui_page_t s_curr_page = EEVEE_PAGE_BADGE;

// Page 0: 工牌主屏组件
static lv_obj_t *s_badge_name;
static lv_obj_t *s_badge_dept_role;
static lv_obj_t *s_badge_auth_tag;
static lv_obj_t *s_badge_capsule;
static lv_obj_t *s_badge_capsule_lbl;

// Page 1: 待办审批组件
static lv_obj_t *s_task_counter_lbl;
static lv_obj_t *s_task_state_lbl;
static lv_obj_t *s_task_title_lbl;
static lv_obj_t *s_task_actions_box;
static lv_obj_t *s_task_btn_approve_lbl;
static lv_obj_t *s_task_btn_reject_lbl;
static lv_obj_t *s_task_empty_lbl;

// Page 2: 业务看板组件
static lv_obj_t *s_rec_num_lbl;
static lv_obj_t *s_rec_summary_lbl;

// Page 3: 名片二维码组件
static lv_obj_t *s_qr_code;
static lv_obj_t *s_qr_hint_lbl;

// Page 4: 设备设置与配网组件
static lv_obj_t *s_set_wifi_lbl;
static lv_obj_t *s_set_server_lbl;
static lv_obj_t *s_set_token_lbl;
static lv_obj_t *s_set_btn_prov;
static lv_obj_t *s_set_btn_prov_lbl;

// 浮动 Toast 组件
static lv_obj_t *s_toast_panel;
static lv_obj_t *s_toast_lbl;
static lv_timer_t *s_toast_timer;

// 热点配网全屏浮层
static lv_obj_t *s_prov_modal;
static lv_obj_t *s_prov_title;
static lv_obj_t *s_prov_ssid_lbl;
static lv_obj_t *s_prov_url_lbl;
static lv_obj_t *s_prov_qr;
static lv_obj_t *s_prov_hint_lbl;
static bool s_prov_visible = false;

static void toast_timer_cb(lv_timer_t *timer)
{
    lv_obj_add_flag(s_toast_panel, LV_OBJ_FLAG_HIDDEN);
    lv_timer_delete(timer);
    s_toast_timer = NULL;
}

void eevee_badge_ui_show_toast(const char *text, uint32_t color_hex)
{
    if (!s_toast_panel || !text) return;
    lv_label_set_text(s_toast_lbl, text);
    lv_obj_set_style_bg_color(s_toast_panel, lv_color_hex(color_hex), 0);
    lv_obj_clear_flag(s_toast_panel, LV_OBJ_FLAG_HIDDEN);

    if (s_toast_timer) {
        lv_timer_delete(s_toast_timer);
        s_toast_timer = NULL;
    }
    s_toast_timer = lv_timer_create(toast_timer_cb, 2200, NULL);
}

static void update_page_indicator(void)
{
    const char *names[EEVEE_PAGE_COUNT] = { "工牌", "待办", "看板", "名片", "设置" };
    lv_label_set_text_fmt(s_lbl_page_indicator, "%d/5 %s",
                          (int)s_curr_page + 1, names[s_curr_page]);
}

void eevee_badge_ui_set_page(eevee_ui_page_t page)
{
    if (page >= EEVEE_PAGE_COUNT) page = EEVEE_PAGE_BADGE;
    s_curr_page = page;
    for (int i = 0; i < EEVEE_PAGE_COUNT; ++i) {
        if (i == (int)s_curr_page) {
            lv_obj_clear_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
    update_page_indicator();
}

void eevee_badge_ui_next_page(void)
{
    eevee_ui_page_t next = (s_curr_page + 1) % EEVEE_PAGE_COUNT;
    eevee_badge_ui_set_page(next);
}

void eevee_badge_ui_prev_page(void)
{
    eevee_ui_page_t prev = (s_curr_page + EEVEE_PAGE_COUNT - 1) % EEVEE_PAGE_COUNT;
    eevee_badge_ui_set_page(prev);
}

eevee_ui_page_t eevee_badge_ui_get_page(void)
{
    return s_curr_page;
}

void eevee_badge_ui_update_status(bool wifi_connected, int battery_soc)
{
    if (wifi_connected) {
        lv_label_set_text(s_lbl_wifi, "● 在线");
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(COLOR_SUCCESS), 0);
    } else {
        lv_label_set_text(s_lbl_wifi, "○ 离线");
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(COLOR_TEXT_MUTED), 0);
    }

    if (battery_soc >= 0) {
        lv_label_set_text_fmt(s_lbl_battery, "%d%%", battery_soc);
    } else {
        lv_label_set_text(s_lbl_battery, "--%");
    }
}

void eevee_badge_ui_update_profile(const eevee_profile_t *profile)
{
    if (!profile || !profile->valid) return;

    if (profile->display_name[0] != '\0') {
        lv_label_set_text(s_badge_name, profile->display_name);
    } else if (profile->username[0] != '\0') {
        lv_label_set_text(s_badge_name, profile->username);
    }

    char role_dept[160];
    snprintf(role_dept, sizeof(role_dept), "%s · %s",
             profile->dept[0] ? profile->dept : "默认组织",
             profile->role[0] ? profile->role : "员工");
    lv_label_set_text(s_badge_dept_role, role_dept);

    if (s_badge_auth_tag) {
        lv_label_set_text(s_badge_auth_tag, "● 认证在职员工");
    }
}

void eevee_badge_ui_update_qr_url(const char *url)
{
    if (!s_qr_code || !url || url[0] == '\0') return;
    lv_qrcode_set_data(s_qr_code, url);
}

void eevee_badge_ui_update_task(const eevee_task_t *task)
{
    if (!task) return;

    // 更新工牌主页的待办胶囊
    if (task->total_count > 0) {
        lv_obj_set_style_bg_color(s_badge_capsule, lv_color_hex(0xFEF3C7), 0);
        lv_obj_set_style_border_color(s_badge_capsule, lv_color_hex(COLOR_WARNING), 0);
        lv_obj_set_style_text_color(s_badge_capsule_lbl, lv_color_hex(0xB45309), 0);
        lv_label_set_text_fmt(s_badge_capsule_lbl, "● 待处理流程 (%d) >", task->total_count);
    } else {
        lv_obj_set_style_bg_color(s_badge_capsule, lv_color_hex(0xD1FAE5), 0);
        lv_obj_set_style_border_color(s_badge_capsule, lv_color_hex(COLOR_SUCCESS), 0);
        lv_obj_set_style_text_color(s_badge_capsule_lbl, lv_color_hex(0x065F46), 0);
        lv_label_set_text(s_badge_capsule_lbl, "✓ 全部流程已完成");
    }

    // 更新 Page 1 待办审批页
    if (task->has_task && task->total_count > 0) {
        lv_obj_clear_flag(s_task_actions_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_task_empty_lbl, LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text_fmt(s_task_counter_lbl, "%d/%d",
                              task->offset + 1, task->total_count);
        lv_label_set_text(s_task_state_lbl,
                          task->state[0] ? task->state : "流转中");
        lv_label_set_text(s_task_title_lbl,
                          task->title[0] ? task->title : "未命名工作流工单");

        lv_label_set_text_fmt(s_task_btn_approve_lbl, "[短按 OK] %s",
                          task->primary_action_name[0] ? task->primary_action_name : "同意审批");
        lv_label_set_text_fmt(s_task_btn_reject_lbl, "[长按 OK] %s",
                          task->reject_action_name[0] ? task->reject_action_name : "驳回修改");
    } else {
        lv_obj_add_flag(s_task_actions_box, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_task_empty_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_task_counter_lbl, "0/0");
        lv_label_set_text(s_task_state_lbl, "--");
        lv_label_set_text(s_task_title_lbl, "");
    }
}

void eevee_badge_ui_update_record(const eevee_record_t *record)
{
    if (!record || !record->valid) return;
    lv_label_set_text_fmt(s_rec_num_lbl, "最新记录 #%d", record->record_number);
    lv_label_set_text(s_rec_summary_lbl,
                      record->summary[0] ? record->summary : "暂无指标数据");
    lv_obj_set_style_text_color(s_rec_summary_lbl, lv_color_hex(COLOR_TEXT_MAIN), 0);
}

static lv_obj_t *create_card_container(lv_obj_t *parent)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(card, 10, 6);
    lv_obj_set_size(card, 220, 280);
    lv_obj_set_style_bg_color(card, lv_color_hex(COLOR_CARD), 0);
    lv_obj_set_style_radius(card, 12, 0);
    lv_obj_set_style_border_color(card, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    return card;
}

static void build_page_badge(lv_obj_t *parent)
{
    lv_obj_t *card = create_card_container(parent);

    // 顶部横幅
    lv_obj_t *banner = lv_obj_create(card);
    lv_obj_remove_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(banner, 0, 0);
    lv_obj_set_size(banner, 204, 28);
    lv_obj_set_style_bg_color(banner, lv_color_hex(COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(banner, 6, 0);
    lv_obj_set_style_pad_all(banner, 0, 0);

    lv_obj_t *b_title = lv_label_create(banner);
    lv_label_set_text(b_title, "EEVEE · 智能工牌");
    lv_obj_set_style_text_font(b_title, eevee_font_get(), 0);
    lv_obj_set_style_text_color(b_title, lv_color_white(), 0);
    lv_obj_set_pos(b_title, 8, 6);

    lv_obj_t *tag = lv_label_create(banner);
    lv_label_set_text(tag, "●在岗");
    lv_obj_set_style_text_font(tag, eevee_font_get(), 0);
    lv_obj_set_style_text_color(tag, lv_color_hex(COLOR_SUCCESS), 0);
    lv_obj_set_pos(tag, 155, 6);

    // 头像相框 (68x68 圆形相框，完美对称居中 x = (204 - 68) / 2 = 68)
    lv_obj_t *avatar_frame = lv_obj_create(card);
    lv_obj_remove_flag(avatar_frame, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(avatar_frame, 68, 36);
    lv_obj_set_size(avatar_frame, 68, 68);
    lv_obj_set_style_radius(avatar_frame, 34, 0);
    lv_obj_set_style_clip_corner(avatar_frame, true, 0);
    lv_obj_set_style_pad_all(avatar_frame, 0, 0);
    lv_obj_set_style_bg_color(avatar_frame, lv_color_hex(0xEFF6FF), 0);
    lv_obj_set_style_border_color(avatar_frame, lv_color_hex(0x93C5FD), 0);
    lv_obj_set_style_border_width(avatar_frame, 2, 0);

    // 头部剪影 (26x26 圆形，水平居中 x = (68 - 26) / 2 = 21, y = 10)
    lv_obj_t *head = lv_obj_create(avatar_frame);
    lv_obj_remove_flag(head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(head, 21, 10);
    lv_obj_set_size(head, 26, 26);
    lv_obj_set_style_radius(head, 13, 0);
    lv_obj_set_style_pad_all(head, 0, 0);
    lv_obj_set_style_border_width(head, 0, 0);
    lv_obj_set_style_bg_color(head, lv_color_hex(0x2563EB), 0);

    // 肩部剪影 (52x32 胶囊形，水平居中 x = (68 - 52) / 2 = 8, y = 40)
    lv_obj_t *shoulder = lv_obj_create(avatar_frame);
    lv_obj_remove_flag(shoulder, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(shoulder, 8, 40);
    lv_obj_set_size(shoulder, 52, 32);
    lv_obj_set_style_radius(shoulder, 16, 0);
    lv_obj_set_style_pad_all(shoulder, 0, 0);
    lv_obj_set_style_border_width(shoulder, 0, 0);
    lv_obj_set_style_bg_color(shoulder, lv_color_hex(0x2563EB), 0);

    // 姓名
    s_badge_name = lv_label_create(card);
    lv_label_set_text(s_badge_name, "企业员工");
    lv_obj_set_style_text_font(s_badge_name, eevee_font_title_get(), 0);
    lv_obj_set_style_text_color(s_badge_name, lv_color_hex(COLOR_TEXT_MAIN), 0);
    lv_obj_set_style_text_align(s_badge_name, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_badge_name, 0, 112);
    lv_obj_set_width(s_badge_name, 204);

    // 部门与职位
    s_badge_dept_role = lv_label_create(card);
    lv_label_set_text(s_badge_dept_role, "组织架构 · 职务");
    lv_obj_set_style_text_font(s_badge_dept_role, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_badge_dept_role, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_text_align(s_badge_dept_role, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_badge_dept_role, 0, 140);
    lv_obj_set_width(s_badge_dept_role, 204);

    // 认证标识胶囊 (水平居中 x = (204 - 116) / 2 = 44)
    lv_obj_t *auth_pill = lv_obj_create(card);
    lv_obj_remove_flag(auth_pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(auth_pill, 44, 166);
    lv_obj_set_size(auth_pill, 116, 24);
    lv_obj_set_style_radius(auth_pill, 12, 0);
    lv_obj_set_style_pad_all(auth_pill, 0, 0);
    lv_obj_set_style_bg_color(auth_pill, lv_color_hex(0xECFDF5), 0);
    lv_obj_set_style_border_color(auth_pill, lv_color_hex(0xA7F3D0), 0);
    lv_obj_set_style_border_width(auth_pill, 1, 0);

    s_badge_auth_tag = lv_label_create(auth_pill);
    lv_label_set_text(s_badge_auth_tag, "● 认证在职员工");
    lv_obj_set_style_text_font(s_badge_auth_tag, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_badge_auth_tag, lv_color_hex(0x059669), 0);
    lv_obj_center(s_badge_auth_tag);

    // 分隔线
    lv_obj_t *line = lv_obj_create(card);
    lv_obj_set_pos(line, 12, 198);
    lv_obj_set_size(line, 180, 1);
    lv_obj_set_style_bg_color(line, lv_color_hex(0xE2E8F0), 0);
    lv_obj_set_style_border_width(line, 0, 0);

    // 待办流程胶囊卡 (水平居中 x = (204 - 184) / 2 = 10)
    s_badge_capsule = lv_obj_create(card);
    lv_obj_remove_flag(s_badge_capsule, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_badge_capsule, 10, 208);
    lv_obj_set_size(s_badge_capsule, 184, 46);
    lv_obj_set_style_radius(s_badge_capsule, 12, 0);
    lv_obj_set_style_pad_all(s_badge_capsule, 0, 0);
    lv_obj_set_style_bg_color(s_badge_capsule, lv_color_hex(0xD1FAE5), 0);
    lv_obj_set_style_border_color(s_badge_capsule, lv_color_hex(COLOR_SUCCESS), 0);
    lv_obj_set_style_border_width(s_badge_capsule, 1, 0);

    s_badge_capsule_lbl = lv_label_create(s_badge_capsule);
    lv_label_set_text(s_badge_capsule_lbl, "✓ 全部流程已完成");
    lv_obj_set_style_text_font(s_badge_capsule_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_badge_capsule_lbl, lv_color_hex(0x065F46), 0);
    lv_obj_center(s_badge_capsule_lbl);
}

static void build_page_tasks(lv_obj_t *parent)
{
    lv_obj_t *card = create_card_container(parent);

    // 顶部横幅
    lv_obj_t *banner = lv_obj_create(card);
    lv_obj_remove_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(banner, 0, 0);
    lv_obj_set_size(banner, 204, 28);
    lv_obj_set_style_bg_color(banner, lv_color_hex(COLOR_TOPBAR), 0);
    lv_obj_set_style_radius(banner, 6, 0);
    lv_obj_set_style_pad_all(banner, 0, 0);

    lv_obj_t *title = lv_label_create(banner);
    lv_label_set_text(title, "待处理流程中心");
    lv_obj_set_style_text_font(title, eevee_font_get(), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 8, 6);

    s_task_counter_lbl = lv_label_create(banner);
    lv_label_set_text(s_task_counter_lbl, "0/0");
    lv_obj_set_style_text_font(s_task_counter_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_task_counter_lbl, lv_color_hex(COLOR_WARNING), 0);
    lv_obj_set_pos(s_task_counter_lbl, 165, 6);

    // 无任务时的空状态展示
    s_task_empty_lbl = lv_label_create(card);
    lv_label_set_text(s_task_empty_lbl, "✓ 暂无待处理流程\n\n当前所有待办已办结");
    lv_obj_set_style_text_font(s_task_empty_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_task_empty_lbl, lv_color_hex(COLOR_SUCCESS), 0);
    lv_obj_set_style_text_align(s_task_empty_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(s_task_empty_lbl);

    // 有任务时的操作区域
    s_task_actions_box = lv_obj_create(card);
    lv_obj_remove_flag(s_task_actions_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_task_actions_box, 0, 32);
    lv_obj_set_size(s_task_actions_box, 204, 236);
    lv_obj_set_style_bg_opa(s_task_actions_box, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_task_actions_box, 0, 0);
    lv_obj_set_style_pad_all(s_task_actions_box, 0, 0);
    lv_obj_add_flag(s_task_actions_box, LV_OBJ_FLAG_HIDDEN);

    // 节点状态胶囊标签
    lv_obj_t *state_pill = lv_obj_create(s_task_actions_box);
    lv_obj_remove_flag(state_pill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(state_pill, 8, 4);
    lv_obj_set_size(state_pill, 100, 22);
    lv_obj_set_style_radius(state_pill, 11, 0);
    lv_obj_set_style_pad_all(state_pill, 0, 0);
    lv_obj_set_style_bg_color(state_pill, lv_color_hex(0xEFF6FF), 0);
    lv_obj_set_style_border_color(state_pill, lv_color_hex(0x93C5FD), 0);
    lv_obj_set_style_border_width(state_pill, 1, 0);

    s_task_state_lbl = lv_label_create(state_pill);
    lv_label_set_text(s_task_state_lbl, "待设备确认");
    lv_obj_set_style_text_font(s_task_state_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_task_state_lbl, lv_color_hex(0x1D4ED8), 0);
    lv_obj_center(s_task_state_lbl);

    // 标题 (充裕垂直空间，支持2-3行完整换行)
    s_task_title_lbl = lv_label_create(s_task_actions_box);
    lv_label_set_text(s_task_title_lbl, "工单标题加载中...");
    lv_label_set_long_mode(s_task_title_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_task_title_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_task_title_lbl, lv_color_hex(COLOR_TEXT_MAIN), 0);
    lv_obj_set_pos(s_task_title_lbl, 8, 32);
    lv_obj_set_width(s_task_title_lbl, 188);

    // 同意按钮条 ([短按 OK] 同意审批)
    lv_obj_t *btn_app = lv_obj_create(s_task_actions_box);
    lv_obj_set_pos(btn_app, 8, 98);
    lv_obj_set_size(btn_app, 188, 46);
    lv_obj_set_style_bg_color(btn_app, lv_color_hex(COLOR_SUCCESS), 0);
    lv_obj_set_style_radius(btn_app, 10, 0);
    lv_obj_set_style_border_width(btn_app, 0, 0);
    s_task_btn_approve_lbl = lv_label_create(btn_app);
    lv_label_set_text(s_task_btn_approve_lbl, "[短按 OK] 同意审批");
    lv_obj_set_style_text_font(s_task_btn_approve_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_task_btn_approve_lbl, lv_color_white(), 0);
    lv_obj_center(s_task_btn_approve_lbl);

    // 驳回按钮条 ([长按 OK] 驳回修改)
    lv_obj_t *btn_rej = lv_obj_create(s_task_actions_box);
    lv_obj_set_pos(btn_rej, 8, 152);
    lv_obj_set_size(btn_rej, 188, 46);
    lv_obj_set_style_bg_color(btn_rej, lv_color_hex(0xFEF2F2), 0);
    lv_obj_set_style_border_color(btn_rej, lv_color_hex(COLOR_DANGER), 0);
    lv_obj_set_style_border_width(btn_rej, 1, 0);
    lv_obj_set_style_radius(btn_rej, 10, 0);
    s_task_btn_reject_lbl = lv_label_create(btn_rej);
    lv_label_set_text(s_task_btn_reject_lbl, "[长按 OK] 驳回修改");
    lv_obj_set_style_text_font(s_task_btn_reject_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_task_btn_reject_lbl, lv_color_hex(COLOR_DANGER), 0);
    lv_obj_center(s_task_btn_reject_lbl);
}

static void build_page_records(lv_obj_t *parent)
{
    lv_obj_t *card = create_card_container(parent);

    // 顶部横幅
    lv_obj_t *banner = lv_obj_create(card);
    lv_obj_remove_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(banner, 0, 0);
    lv_obj_set_size(banner, 204, 28);
    lv_obj_set_style_bg_color(banner, lv_color_hex(COLOR_TEAL), 0);
    lv_obj_set_style_radius(banner, 6, 0);
    lv_obj_set_style_pad_all(banner, 0, 0);

    lv_obj_t *title = lv_label_create(banner);
    lv_label_set_text(title, "业务记录看板");
    lv_obj_set_style_text_font(title, eevee_font_get(), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 8, 6);

    // 记录编号
    s_rec_num_lbl = lv_label_create(card);
    lv_label_set_text(s_rec_num_lbl, "最新记录 #--");
    lv_obj_set_style_text_font(s_rec_num_lbl, eevee_font_title_get(), 0);
    lv_obj_set_style_text_color(s_rec_num_lbl, lv_color_hex(COLOR_TEXT_MAIN), 0);
    lv_obj_set_pos(s_rec_num_lbl, 8, 36);

    // 摘要面板
    lv_obj_t *summary_box = lv_obj_create(card);
    lv_obj_remove_flag(summary_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(summary_box, 8, 68);
    lv_obj_set_size(summary_box, 188, 104);
    lv_obj_set_style_bg_color(summary_box, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_border_color(summary_box, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(summary_box, 1, 0);
    lv_obj_set_style_radius(summary_box, 8, 0);

    s_rec_summary_lbl = lv_label_create(summary_box);
    lv_label_set_text(s_rec_summary_lbl, "暂未同步业务记录\n按 OK 键刷新或上报");
    lv_label_set_long_mode(s_rec_summary_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_rec_summary_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_rec_summary_lbl, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(s_rec_summary_lbl, 6, 6);
    lv_obj_set_width(s_rec_summary_lbl, 176);

    // 打卡上报操作按钮 ([短按 OK] 随身打卡上报)
    lv_obj_t *btn_act = lv_obj_create(card);
    lv_obj_set_pos(btn_act, 8, 182);
    lv_obj_set_size(btn_act, 188, 46);
    lv_obj_set_style_bg_color(btn_act, lv_color_hex(0x6366F1), 0); // Indigo
    lv_obj_set_style_radius(btn_act, 8, 0);
    lv_obj_set_style_border_width(btn_act, 0, 0);

    lv_obj_t *act_lbl = lv_label_create(btn_act);
    lv_label_set_text(act_lbl, "[短按 OK] 随身打卡上报");
    lv_obj_set_style_text_font(act_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(act_lbl, lv_color_white(), 0);
    lv_obj_center(act_lbl);
}

static void build_page_qr(lv_obj_t *parent)
{
    lv_obj_t *card = create_card_container(parent);

    // 顶部横幅
    lv_obj_t *banner = lv_obj_create(card);
    lv_obj_remove_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(banner, 0, 0);
    lv_obj_set_size(banner, 204, 28);
    lv_obj_set_style_bg_color(banner, lv_color_hex(0x334155), 0);
    lv_obj_set_style_radius(banner, 6, 0);
    lv_obj_set_style_pad_all(banner, 0, 0);

    lv_obj_t *title = lv_label_create(banner);
    lv_label_set_text(title, "员工电子名片码");
    lv_obj_set_style_text_font(title, eevee_font_get(), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 8, 6);

    // 二维码组件 (140x140，水平居中 x = (204 - 140) / 2 = 32)
    s_qr_code = lv_qrcode_create(card);
    lv_qrcode_set_size(s_qr_code, 140);
    lv_qrcode_set_dark_color(s_qr_code, lv_color_hex(COLOR_TEXT_MAIN));
    lv_qrcode_set_light_color(s_qr_code, lv_color_white());
    lv_qrcode_set_quiet_zone(s_qr_code, true);
    lv_qrcode_set_data(s_qr_code, "http://192.168.8.100:3000/settings/profile");
    lv_obj_set_pos(s_qr_code, 32, 40);

    s_qr_hint_lbl = lv_label_create(card);
    lv_label_set_text(s_qr_hint_lbl, "微信或手机扫码直接访问\n员工个人资料与设置页");
    lv_label_set_long_mode(s_qr_hint_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_qr_hint_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_qr_hint_lbl, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_style_text_align(s_qr_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_qr_hint_lbl, 0, 196);
    lv_obj_set_width(s_qr_hint_lbl, 204);
}

static void build_page_settings(lv_obj_t *parent)
{
    lv_obj_t *card = create_card_container(parent);

    // 顶部横幅
    lv_obj_t *banner = lv_obj_create(card);
    lv_obj_remove_flag(banner, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(banner, 0, 0);
    lv_obj_set_size(banner, 204, 28);
    lv_obj_set_style_bg_color(banner, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_radius(banner, 6, 0);
    lv_obj_set_style_pad_all(banner, 0, 0);

    lv_obj_t *title = lv_label_create(banner);
    lv_label_set_text(title, "设备设置与配网");
    lv_obj_set_style_text_font(title, eevee_font_get(), 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_set_pos(title, 8, 6);

    // 信息展示卡 (flex column 垂直流式排列，避免绝对坐标导致的换行重叠)
    lv_obj_t *info_box = lv_obj_create(card);
    lv_obj_remove_flag(info_box, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(info_box, 8, 36);
    lv_obj_set_size(info_box, 188, 122);
    lv_obj_set_style_bg_color(info_box, lv_color_hex(0xF8FAFC), 0);
    lv_obj_set_style_border_color(info_box, lv_color_hex(COLOR_BORDER), 0);
    lv_obj_set_style_border_width(info_box, 1, 0);
    lv_obj_set_style_radius(info_box, 10, 0);
    lv_obj_set_style_pad_all(info_box, 8, 0);
    lv_obj_set_flex_flow(info_box, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(info_box, 6, 0);

    s_set_wifi_lbl = lv_label_create(info_box);
    lv_label_set_text(s_set_wifi_lbl, "网络: 检测中...");
    lv_obj_set_style_text_font(s_set_wifi_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_set_wifi_lbl, lv_color_hex(COLOR_TEXT_MAIN), 0);

    s_set_server_lbl = lv_label_create(info_box);
    lv_label_set_text(s_set_server_lbl, "服务: 192.168.8.100:3001");
    lv_obj_set_style_text_font(s_set_server_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_set_server_lbl, lv_color_hex(COLOR_TEXT_MUTED), 0);

    s_set_token_lbl = lv_label_create(info_box);
    lv_label_set_text(s_set_token_lbl, "令牌: 已就绪 (PAT)");
    lv_obj_set_style_text_font(s_set_token_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_set_token_lbl, lv_color_hex(COLOR_PRIMARY), 0);

    lv_obj_t *fw_lbl = lv_label_create(info_box);
    lv_label_set_text(fw_lbl, "系统: Eevee Badge v1.0");
    lv_obj_set_style_text_font(fw_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(fw_lbl, lv_color_hex(COLOR_TEXT_MUTED), 0);

    // 核心操作按钮：精简文字为 [短按 OK] 重新配网
    s_set_btn_prov = lv_obj_create(card);
    lv_obj_remove_flag(s_set_btn_prov, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_set_btn_prov, 8, 172);
    lv_obj_set_size(s_set_btn_prov, 188, 48);
    lv_obj_set_style_bg_color(s_set_btn_prov, lv_color_hex(COLOR_PRIMARY), 0);
    lv_obj_set_style_radius(s_set_btn_prov, 10, 0);
    lv_obj_set_style_border_width(s_set_btn_prov, 0, 0);

    s_set_btn_prov_lbl = lv_label_create(s_set_btn_prov);
    lv_label_set_text(s_set_btn_prov_lbl, "[短按 OK] 重新配网");
    lv_obj_set_style_text_font(s_set_btn_prov_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_set_btn_prov_lbl, lv_color_white(), 0);
    lv_obj_center(s_set_btn_prov_lbl);
}

static void build_prov_modal(lv_obj_t *parent)
{
    s_prov_modal = lv_obj_create(parent);
    lv_obj_remove_flag(s_prov_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_prov_modal, 8, 28);
    lv_obj_set_size(s_prov_modal, 224, 284);
    lv_obj_set_style_bg_color(s_prov_modal, lv_color_hex(0x0F172A), 0);
    lv_obj_set_style_radius(s_prov_modal, 12, 0);
    lv_obj_set_style_border_color(s_prov_modal, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_border_width(s_prov_modal, 2, 0);
    lv_obj_set_style_pad_all(s_prov_modal, 8, 0);
    lv_obj_add_flag(s_prov_modal, LV_OBJ_FLAG_HIDDEN);

    s_prov_title = lv_label_create(s_prov_modal);
    lv_label_set_text(s_prov_title, "● 热点配网模式");
    lv_obj_set_style_text_font(s_prov_title, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_prov_title, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_pos(s_prov_title, 8, 6);

    s_prov_ssid_lbl = lv_label_create(s_prov_modal);
    lv_label_set_text(s_prov_ssid_lbl, "热点: Eevee-Badge-Setup");
    lv_obj_set_style_text_font(s_prov_ssid_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_prov_ssid_lbl, lv_color_white(), 0);
    lv_obj_set_pos(s_prov_ssid_lbl, 8, 28);

    s_prov_qr = lv_qrcode_create(s_prov_modal);
    lv_qrcode_set_size(s_prov_qr, 110);
    lv_qrcode_set_dark_color(s_prov_qr, lv_color_hex(0x0F172A));
    lv_qrcode_set_light_color(s_prov_qr, lv_color_white());
    lv_qrcode_set_quiet_zone(s_prov_qr, true);
    lv_qrcode_set_data(s_prov_qr, "WIFI:T:nopass;S:Eevee-Badge-Setup;;;");
    lv_obj_set_pos(s_prov_qr, 48, 50);

    s_prov_url_lbl = lv_label_create(s_prov_modal);
    lv_label_set_text(s_prov_url_lbl, "网页打开: 192.168.4.1");
    lv_obj_set_style_text_font(s_prov_url_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_prov_url_lbl, lv_color_hex(0x38BDF8), 0);
    lv_obj_set_style_text_align(s_prov_url_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_prov_url_lbl, 0, 168);
    lv_obj_set_width(s_prov_url_lbl, 208);

    s_prov_hint_lbl = lv_label_create(s_prov_modal);
    lv_label_set_text(s_prov_hint_lbl, "手机扫码加入热点完成配置\n[短按 OK 键] 退出配网");
    lv_label_set_long_mode(s_prov_hint_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_font(s_prov_hint_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_prov_hint_lbl, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_text_align(s_prov_hint_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_prov_hint_lbl, 0, 198);
    lv_obj_set_width(s_prov_hint_lbl, 208);
}

void eevee_badge_ui_init(void)
{
    s_main_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_main_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_main_scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_border_width(s_main_scr, 0, 0);
    lv_obj_set_style_pad_all(s_main_scr, 0, 0);

    // 1. 全局状态栏 (0 to 24px)
    s_topbar = lv_obj_create(s_main_scr);
    lv_obj_remove_flag(s_topbar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_topbar, 0, 0);
    lv_obj_set_size(s_topbar, SCREEN_W, 24);
    lv_obj_set_style_bg_color(s_topbar, lv_color_hex(COLOR_TOPBAR), 0);
    lv_obj_set_style_border_width(s_topbar, 0, 0);
    lv_obj_set_style_pad_all(s_topbar, 0, 0);

    s_lbl_wifi = lv_label_create(s_topbar);
    lv_label_set_text(s_lbl_wifi, "○ 离线");
    lv_obj_set_style_text_font(s_lbl_wifi, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(COLOR_TEXT_MUTED), 0);
    lv_obj_set_pos(s_lbl_wifi, 10, 4);

    s_lbl_page_indicator = lv_label_create(s_topbar);
    lv_label_set_text(s_lbl_page_indicator, "1/5 工牌");
    lv_obj_set_style_text_font(s_lbl_page_indicator, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_lbl_page_indicator, lv_color_hex(0x94A3B8), 0);
    lv_obj_center(s_lbl_page_indicator);

    s_lbl_battery = lv_label_create(s_topbar);
    lv_label_set_text(s_lbl_battery, "--%");
    lv_obj_set_style_text_font(s_lbl_battery, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_lbl_battery, lv_color_white(), 0);
    lv_obj_set_pos(s_lbl_battery, 198, 4);

    // 2. 主视口与 5 页面创建
    lv_obj_t *viewport = lv_obj_create(s_main_scr);
    lv_obj_remove_flag(viewport, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(viewport, 0, 24);
    lv_obj_set_size(viewport, SCREEN_W, SCREEN_H - 24);
    lv_obj_set_style_bg_opa(viewport, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(viewport, 0, 0);
    lv_obj_set_style_pad_all(viewport, 0, 0);

    for (int i = 0; i < EEVEE_PAGE_COUNT; ++i) {
        s_pages[i] = lv_obj_create(viewport);
        lv_obj_remove_flag(s_pages[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(s_pages[i], 0, 0);
        lv_obj_set_size(s_pages[i], SCREEN_W, SCREEN_H - 24);
        lv_obj_set_style_bg_opa(s_pages[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_pages[i], 0, 0);
        lv_obj_set_style_pad_all(s_pages[i], 0, 0);
    }

    build_page_badge(s_pages[EEVEE_PAGE_BADGE]);
    build_page_tasks(s_pages[EEVEE_PAGE_TASKS]);
    build_page_records(s_pages[EEVEE_PAGE_RECORDS]);
    build_page_qr(s_pages[EEVEE_PAGE_QR]);
    build_page_settings(s_pages[EEVEE_PAGE_SETTINGS]);

    // 3. 浮动 Toast 弹窗 (默认隐藏)
    s_toast_panel = lv_obj_create(s_main_scr);
    lv_obj_remove_flag(s_toast_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_toast_panel, 20, 130);
    lv_obj_set_size(s_toast_panel, 200, 50);
    lv_obj_set_style_radius(s_toast_panel, 10, 0);
    lv_obj_set_style_bg_color(s_toast_panel, lv_color_hex(COLOR_SUCCESS), 0);
    lv_obj_set_style_border_width(s_toast_panel, 0, 0);
    lv_obj_add_flag(s_toast_panel, LV_OBJ_FLAG_HIDDEN);

    s_toast_lbl = lv_label_create(s_toast_panel);
    lv_label_set_text(s_toast_lbl, "操作成功");
    lv_obj_set_style_text_font(s_toast_lbl, eevee_font_get(), 0);
    lv_obj_set_style_text_color(s_toast_lbl, lv_color_white(), 0);
    // 4. 热点配网浮层面板 (默认隐藏)
    build_prov_modal(s_main_scr);

    eevee_badge_ui_set_page(EEVEE_PAGE_BADGE);
    lv_screen_load(s_main_scr);
}

void eevee_badge_ui_update_settings(const char *wifi_ssid, bool connected,
                                    const char *server_url, bool has_token)
{
    if (s_set_wifi_lbl) {
        lv_label_set_text_fmt(s_set_wifi_lbl, "网络: %s (%s)",
                              (wifi_ssid && wifi_ssid[0]) ? wifi_ssid : "未配置",
                              connected ? "在线" : "离线");
        lv_obj_set_style_text_color(s_set_wifi_lbl,
                                    connected ? lv_color_hex(COLOR_SUCCESS) : lv_color_hex(COLOR_TEXT_MUTED),
                                    0);
    }
    if (s_set_server_lbl && server_url && server_url[0]) {
        const char *display_host = server_url;
        if (strncmp(display_host, "http://", 7) == 0) {
            display_host += 7;
        } else if (strncmp(display_host, "https://", 8) == 0) {
            display_host += 8;
        }
        lv_label_set_text_fmt(s_set_server_lbl, "服务: %s", display_host);
    }
    if (s_set_token_lbl) {
        lv_label_set_text(s_set_token_lbl, has_token ? "令牌: 已就绪 (PAT)" : "令牌: 未配置 ⚠️");
        lv_obj_set_style_text_color(s_set_token_lbl,
                                    has_token ? lv_color_hex(COLOR_PRIMARY) : lv_color_hex(COLOR_WARNING),
                                    0);
    }
}

void eevee_badge_ui_show_prov_view(bool visible, const char *ap_ssid, const char *web_url)
{
    s_prov_visible = visible;
    if (!s_prov_modal) return;
    if (visible) {
        if (ap_ssid && ap_ssid[0]) {
            lv_label_set_text_fmt(s_prov_ssid_lbl, "热点: %s", ap_ssid);
            char wifi_qr[128];
            snprintf(wifi_qr, sizeof(wifi_qr), "WIFI:T:nopass;S:%s;;;", ap_ssid);
            lv_qrcode_set_data(s_prov_qr, wifi_qr);
        }
        if (web_url && web_url[0]) {
            lv_label_set_text_fmt(s_prov_url_lbl, "网页打开: %s", web_url);
        }
        lv_label_set_text(s_lbl_wifi, "● 配网模式");
        lv_obj_set_style_text_color(s_lbl_wifi, lv_color_hex(COLOR_WARNING), 0);
        lv_obj_clear_flag(s_prov_modal, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_prov_modal, LV_OBJ_FLAG_HIDDEN);
        update_page_indicator();
    }
}

bool eevee_badge_ui_is_prov_view_visible(void)
{
    return s_prov_visible;
}
