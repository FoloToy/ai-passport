// main/app_lucky_draw.c —— 快乐选号 UI:三个主页面 + 两个开奖子页。
// 页面结构为本应用全新设计(非基线 demo 外壳):顶部两个主玩法标签页
// (快乐选号 / 双色球),当前玩法亮色+底部色条;开奖是各玩法的子页,不占标签位。
// 单 screen 多容器切换,无后台任务;lv_timer 仅刷新电量标签。
//
// 按键映射:
//   UP/DOWN 单击  设置页调整号码个数(1..9,双色球无效);开奖页翻期
//   OK 单击       设置页=开始抽取;结果页=再抽一组;开奖页=刷新
//   OK 长按       设置页=切换主玩法;结果页/开奖页=返回设置页
//   DOWN 长按     设置页=看当前玩法的官方开奖(快乐8 / 双色球);开奖页=返回
//   UP 长按       进入手机网页配网(换 WiFi 不用重编固件);配网页里 OK 长按退出
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "kl8_result.h"
#include "lucky_draw_logic.h"
#include "net_wifi.h"
#include "ssq_result.h"
#include "wifi_prov.h"
#include "lvgl.h"
#include "esp_random.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "lucky_draw";

LV_FONT_DECLARE(lucky_font_16);
LV_FONT_DECLARE(lucky_font_20);
LV_FONT_DECLARE(lucky_font_24);
LV_FONT_DECLARE(lucky_font_40);

// 配色:深蓝底、白字,号码球按位置循环六色(与抽取顺序无关,仅装饰)。
// 双色球模式:红球统一红、蓝球统一蓝。
#define COLOR_BG      0x14212B
#define COLOR_TEXT    0xF2F5F7
#define COLOR_SUBTLE  0x8FA3B0
#define COLOR_ACCENT  0x2E86DE
#define COLOR_TAB_DIM 0x5A6B7C
#define COLOR_WARN     0xEF9F27   // 需用户注意的提示(2.4G / 未联网)
#define COLOR_SSQ_RED   0xD8402F
#define COLOR_SSQ_BLUE  0x2E6FDE
#define BALL_COLORS   6
static const uint32_t BALL_PALETTE[BALL_COLORS] = {
    0xE24B4B, 0xEF9F27, 0x97C459, 0x378ADD, 0x7F77DD, 0x1D9E75,
};

static lv_obj_t *s_scr;
static lv_obj_t *s_setup_panel;
static lv_obj_t *s_result_panel;
static lv_obj_t *s_issues_panel;
static lv_obj_t *s_tabs;                       // 主玩法标签容器
static lv_obj_t *s_tab[2];                     // 0=快乐选号 1=双色球
static lv_obj_t *s_tab_bar[2];
static lv_obj_t *s_hint_keys;                  // 设置页底部提示(主操作)
static lv_obj_t *s_hint_mode;                  // 设置页底部第二行(长按 OK 换玩法)
static lv_obj_t *s_hint_pick;
static lv_obj_t *s_arrow_up;
static lv_obj_t *s_arrow_down;
static lv_obj_t *s_count_ball;
static lv_obj_t *s_count_label;
static lv_obj_t *s_count_summary;
static lv_obj_t *s_balls[LUCKY_COUNT_MAX];
static lv_obj_t *s_issue_code;                 // 开奖页:期号
static lv_obj_t *s_issue_date;                 // 开奖页:日期 + 序位
static lv_obj_t *s_issue_status;               // 开奖页:加载/失败提示
static lv_obj_t *s_issue_balls[LUCKY_SSQ_TOTAL];
static lv_obj_t *s_kl8_panel;                  // 快乐8开奖页
static lv_obj_t *s_kl8_kicker;                 // "快乐8开奖 · 中 N 个"
static lv_obj_t *s_kl8_code;
static lv_obj_t *s_kl8_date;
static lv_obj_t *s_kl8_status;
static lv_obj_t *s_kl8_balls[KL8_NUM_COUNT];
static lv_obj_t *s_battery_label;
static lv_timer_t *s_battery_timer;
static lv_obj_t *s_prov_panel;                 // 配网页(覆盖全屏,长按上键进入)
static lv_obj_t *s_prov_ap;                    // 配网页:热点名
static lv_obj_t *s_prov_state;                 // 配网页:等待提交 / 已保存
static lv_timer_t *s_prov_timer;               // 配网页状态刷新(1 秒)
static bool s_prov_open;
static lucky_state_t s_state;

// 标签页布局:两个主玩法固定位置,只有高亮与色条随玩法切换。
// 字体 20px,宽度 80/60 -> 中心 x = 90 / 166。开奖结果不占标签位,它是双色球的子页。
static const int TAB_CX[2] = {90, 166};
static const int TAB_BAR_W[2] = {56, 44};
static const char *TAB_TEXT[2] = {"快乐选号", "双色球"};
static const uint32_t TAB_BAR_COLOR[2] = {0x1D9E75, COLOR_SSQ_RED};

// 电量:读失败优雅降级;低电量(<20%)标红。lv_timer 跑在 LVGL 任务内,已持锁。
static void battery_tick(lv_timer_t *t) {
    (void)t;
    if (!s_battery_label) return;
    int soc = bsp_battery_soc();
    lv_color_t color = lv_color_hex(COLOR_SUBTLE);
    char text[16];
    if (soc < 0) {
        strcpy(text, "-- %");
    } else {
        snprintf(text, sizeof(text), "%d %%", soc);
        if (soc < 20) color = lv_color_hex(0xFF5A5A);
    }
    lv_label_set_text(s_battery_label, text);
    lv_obj_set_style_text_color(s_battery_label, color, 0);
}

static lv_obj_t *make_label(lv_obj_t *parent, const lv_font_t *font, uint32_t color) {
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
    lv_label_set_text(label, "");
    return label;
}

// 号码球:圆形容器,数字居中。重建内容直接改样式和文本,不销毁对象。
// 双色球模式下:前 6 球统一红、第 7 球蓝,覆盖六色轮换。
static void ball_render(int index, uint8_t value) {
    lv_obj_t *ball = s_balls[index];
    uint32_t color = BALL_PALETTE[index % BALL_COLORS];
    if (s_state.mode == LUCKY_MODE_SSQ) {
        color = (index < LUCKY_SSQ_RED_COUNT) ? COLOR_SSQ_RED : COLOR_SSQ_BLUE;
    }
    lv_obj_clear_flag(ball, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(ball, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_t *num = (lv_obj_t *)lv_obj_get_user_data(ball);
    char text[4];
    snprintf(text, sizeof(text), "%d", value);
    lv_label_set_text(num, text);
}

static void balls_layout(void) {
    // 先隐藏全部球再按 nums_len 显示:否则上一次大个数抽取的球会残留。
    for (int i = 0; i < LUCKY_COUNT_MAX; i++) {
        lv_obj_add_flag(s_balls[i], LV_OBJ_FLAG_HIDDEN);
    }
    int count = s_state.nums_len;
    const int ball_d = 52;
    const int cy0 = 118;                       // 第一行中心
    const int cy_step = 62;

    // 每行最多 3 球:1 球行放中线,2 球行对称,3 球行固定三列。
    int idx = 0;
    int row = 0;
    while (count > 0 && idx < LUCKY_COUNT_MAX) {
        int in_row = count > 3 ? 3 : count;
        for (int c = 0; c < in_row; c++) {
            int cx;
            if (in_row == 1) cx = 120;
            else if (in_row == 2) cx = 91 + c * 58;
            else cx = 62 + c * 58;
            lv_obj_t *ball = s_balls[idx];
            lv_obj_set_pos(ball, cx - ball_d / 2, cy0 + row * cy_step - ball_d / 2);
            ball_render(idx, s_state.nums[idx]);
            idx++;
        }
        count -= in_row;
        row++;
    }
    char text[24];
    if (s_state.mode == LUCKY_MODE_SSQ) {
        snprintf(text, sizeof(text), "红球 %d · 蓝球 %d", LUCKY_SSQ_RED_COUNT, LUCKY_SSQ_BLUE_COUNT);
    } else {
        snprintf(text, sizeof(text), "本组 %d 个号码", s_state.nums_len);
    }
    lv_label_set_text(s_count_summary, text);
}

// 标签页同步:当前玩法白字+色条,其余置灰。
// 开奖页属于双色球,高亮落在双色球标签上。
static void tabs_sync(void) {
    int active = (s_state.mode == LUCKY_MODE_LUCKY) ? 0 : 1;
    for (int i = 0; i < 2; i++) {
        lv_obj_set_style_text_color(s_tab[i],
                                    lv_color_hex(i == active ? COLOR_TEXT : COLOR_TAB_DIM), 0);
        if (i == active) {
            lv_obj_clear_flag(s_tab_bar[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_tab_bar[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// 开奖页无数据时的状态说明。
// 离线时给第二行提示:联网方式就是长按上键开配网,不用去翻说明书。
static const char *offline_text(void) {
    return net_wifi_configured() ? "网络未连接\n长按上键配网" : "未配置网络\n长按上键配网";
}

static const char *issue_status_text(void) {
    switch (ssq_result_state()) {
        case SSQ_STATE_LOADING: return "加载中";
        case SSQ_STATE_NO_WIFI: return offline_text();
        case SSQ_STATE_FAIL:    return "拉取失败";
        default:                return "暂无数据";
    }
}

// 开奖页渲染:一屏一期(期号 / 日期 / 6 红 + 1 蓝)。
static void issues_render(void) {
    int n = ssq_result_count();
    s_state.issue_count = n;
    if (s_state.issue_idx >= n) s_state.issue_idx = (n > 0) ? n - 1 : 0;
    const ssq_issue_t *issue = ssq_result_at(s_state.issue_idx);

    if (issue == NULL) {
        for (int i = 0; i < LUCKY_SSQ_TOTAL; i++) {
            lv_obj_add_flag(s_issue_balls[i], LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(s_issue_code, "");
        lv_label_set_text(s_issue_date, "");
        lv_label_set_text(s_issue_status, issue_status_text());
        lv_obj_clear_flag(s_issue_status, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(s_issue_status, LV_OBJ_FLAG_HIDDEN);
    char text[48];   // 期号/日期拼接 + 两个整数的理论最大长度都放得下
    snprintf(text, sizeof(text), "%s 期", issue->code);
    lv_label_set_text(s_issue_code, text);
    snprintf(text, sizeof(text), "%s · %d/%d", issue->date, s_state.issue_idx + 1, n);
    lv_label_set_text(s_issue_date, text);

    // 红球两行各 3 个,蓝球单独居中一行。
    static const int row_cx[3] = {58, 120, 182};
    static const int row_cy[2] = {170, 222};
    for (int i = 0; i < LUCKY_SSQ_RED_COUNT; i++) {
        lv_obj_t *ball = s_issue_balls[i];
        lv_obj_clear_flag(ball, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(ball, lv_color_hex(COLOR_SSQ_RED), LV_PART_MAIN);
        lv_obj_set_pos(ball, row_cx[i % 3] - 22, row_cy[i / 3] - 22);
        lv_obj_t *num = (lv_obj_t *)lv_obj_get_user_data(ball);
        snprintf(text, sizeof(text), "%d", issue->red[i]);
        lv_label_set_text(num, text);
    }
    lv_obj_t *blue = s_issue_balls[LUCKY_SSQ_RED_COUNT];
    lv_obj_clear_flag(blue, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(blue, lv_color_hex(COLOR_SSQ_BLUE), LV_PART_MAIN);
    lv_obj_set_pos(blue, 120 - 22, 274 - 22);
    lv_obj_t *blue_num = (lv_obj_t *)lv_obj_get_user_data(blue);
    snprintf(text, sizeof(text), "%d", issue->blue);
    lv_label_set_text(blue_num, text);
}

// 快乐8开奖页无数据时的状态说明。
static const char *kl8_status_text(void) {
    switch (kl8_result_state()) {
        case KL8_STATE_LOADING: return "加载中";
        case KL8_STATE_NO_WIFI: return offline_text();
        case KL8_STATE_FAIL:    return "拉取失败";
        default:                return "暂无数据";
    }
}

// 快乐8开奖页:一屏一期(标题+命中数 / 期号 / 日期 / 20 个号)。
// 自选号命中开奖号的球换成金色,配合标题里的「中 N 个」做对照。
static void kl8_render(void) {
    int n = kl8_result_count();
    s_state.issue_count = n;
    if (s_state.issue_idx >= n) s_state.issue_idx = (n > 0) ? n - 1 : 0;
    const kl8_issue_t *issue = kl8_result_at(s_state.issue_idx);

    if (issue == NULL) {
        for (int i = 0; i < KL8_NUM_COUNT; i++) {
            lv_obj_add_flag(s_kl8_balls[i], LV_OBJ_FLAG_HIDDEN);
        }
        lv_label_set_text(s_kl8_kicker, "快乐8开奖");
        lv_obj_set_style_text_color(s_kl8_kicker, lv_color_hex(COLOR_SSQ_RED), 0);
        lv_label_set_text(s_kl8_code, "");
        lv_label_set_text(s_kl8_date, "");
        lv_label_set_text(s_kl8_status, kl8_status_text());
        lv_obj_clear_flag(s_kl8_status, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_obj_add_flag(s_kl8_status, LV_OBJ_FLAG_HIDDEN);
    char text[48];
    snprintf(text, sizeof(text), "%s 期", issue->code);
    lv_label_set_text(s_kl8_code, text);
    snprintf(text, sizeof(text), "%s · %d/%d", issue->date, s_state.issue_idx + 1, n);
    lv_label_set_text(s_kl8_date, text);

    // 4 行 x 5 列,球径 30,列距 47、行距 36。
    const int cx0 = 26, cx_step = 47;
    const int cy0 = 170, cy_step = 36;
    int hits = 0;
    for (int i = 0; i < KL8_NUM_COUNT; i++) {
        uint8_t v = issue->nums[i];
        bool hit = false;
        for (int k = 0; k < s_state.nums_len && k < LUCKY_COUNT_MAX; k++) {
            if (s_state.nums[k] == v) {
                hit = true;
                break;
            }
        }
        if (hit && s_state.nums_len > 0) hits++;

        lv_obj_t *ball = s_kl8_balls[i];
        lv_obj_clear_flag(ball, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(ball, lv_color_hex(hit ? 0xEF9F27 : COLOR_ACCENT), LV_PART_MAIN);
        lv_obj_set_pos(ball, cx0 + (i % 5) * cx_step - 15, cy0 + (i / 5) * cy_step - 15);
        lv_obj_t *num = (lv_obj_t *)lv_obj_get_user_data(ball);
        snprintf(text, sizeof(text), "%d", v);
        lv_label_set_text(num, text);
    }

    if (s_state.nums_len > 0) {
        snprintf(text, sizeof(text), "快乐8开奖 · 中 %d 个", hits);
        lv_obj_set_style_text_color(s_kl8_kicker, lv_color_hex(hits > 0 ? 0xEF9F27 : COLOR_SUBTLE), 0);
    } else {
        snprintf(text, sizeof(text), "快乐8开奖");
        lv_obj_set_style_text_color(s_kl8_kicker, lv_color_hex(COLOR_SSQ_RED), 0);
    }
    lv_label_set_text(s_kl8_kicker, text);
}

static void page_switch(lucky_page_t page) {
    bool setup = (page == LUCKY_PAGE_SETUP);
    bool result = (page == LUCKY_PAGE_RESULT);
    bool issues = (page == LUCKY_PAGE_ISSUES);
    bool kl8 = (page == LUCKY_PAGE_KL8);

    if (setup) lv_obj_clear_flag(s_setup_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_setup_panel, LV_OBJ_FLAG_HIDDEN);
    if (result) lv_obj_clear_flag(s_result_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_result_panel, LV_OBJ_FLAG_HIDDEN);
    if (issues) lv_obj_clear_flag(s_issues_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_issues_panel, LV_OBJ_FLAG_HIDDEN);
    if (kl8) lv_obj_clear_flag(s_kl8_panel, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(s_kl8_panel, LV_OBJ_FLAG_HIDDEN);

    // 结果页顶部要放汇总文案,标签页在那里隐藏;其余页面常显。
    if (result) lv_obj_add_flag(s_tabs, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_clear_flag(s_tabs, LV_OBJ_FLAG_HIDDEN);
    tabs_sync();

    if (issues) {
        issues_render();
        return;
    }
    if (kl8) {
        kl8_render();
        return;
    }
    if (result) {
        balls_layout();
        return;
    }

    bool ssq = (s_state.mode == LUCKY_MODE_SSQ);
    lv_label_set_text(s_hint_pick, ssq ? "红球 33 选 6 · 蓝球 16 选 1" : "选择号码个数");
    // 开奖入口两个玩法一致:长按下键看对应官方开奖;换玩法是另一颗键,单独一行写清。
    lv_label_set_text(s_hint_keys, "确定开始 · 长按下键看开奖");
    // 没联网时最后一行改成配网入口:开奖离线不可用,用户得先知道怎么联网。
    if (net_wifi_connected()) {
        lv_label_set_text(s_hint_mode, "长按 OK 切换玩法");
        lv_obj_set_style_text_color(s_hint_mode, lv_color_hex(COLOR_TAB_DIM), 0);
    } else {
        lv_label_set_text(s_hint_mode, "未联网 · 长按上键配网");
        lv_obj_set_style_text_color(s_hint_mode, lv_color_hex(COLOR_WARN), 0);
    }
    if (ssq) {
        // 双色球个数固定 6+1:隐藏箭头,大彩球红蓝垂直渐变(上红下蓝)。
        lv_obj_add_flag(s_arrow_up, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_arrow_down, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(s_count_ball, lv_color_hex(COLOR_SSQ_RED), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_color(s_count_ball, lv_color_hex(COLOR_SSQ_BLUE), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(s_count_ball, LV_GRAD_DIR_VER, LV_PART_MAIN);
        lv_label_set_text(s_count_label, "6+1");
    } else {
        // 快乐选号:个数球颜色随个数轮换,调整时有色彩反馈。
        lv_obj_clear_flag(s_arrow_up, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_arrow_down, LV_OBJ_FLAG_HIDDEN);
        uint32_t c = BALL_PALETTE[(s_state.count - 1) % BALL_COLORS];
        // 切回快乐选号要关掉双色球留下的渐变。
        lv_obj_set_style_bg_grad_dir(s_count_ball, LV_GRAD_DIR_NONE, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_count_ball, lv_color_hex(c), LV_PART_MAIN);
        char text[8];
        snprintf(text, sizeof(text), "%d", s_state.count);
        lv_label_set_text(s_count_label, text);
    }
}

static void draw_and_show(void) {
    if (s_state.mode == LUCKY_MODE_SSQ) {
        if (!lucky_draw_ssq(esp_random, s_state.nums)) {
            ESP_LOGE(TAG, "双色球抽取失败");
            return;
        }
        s_state.nums_len = LUCKY_SSQ_TOTAL;
    } else {
        if (!lucky_draw_numbers(esp_random, s_state.count, s_state.nums)) {
            ESP_LOGE(TAG, "抽取失败: count=%d", s_state.count);
            return;
        }
        s_state.nums_len = s_state.count;
    }
    balls_layout();
}

// ---- 配网页 ----
// 换 WiFi 不重编固件:设备开热点,手机连上填 SSID/密码,存进 NVS 后重启生效。
// 页面状态每秒从配网模块取一次,不做跨任务回调,避免和 LVGL 抢锁。
static void prov_tick(lv_timer_t *t) {
    (void)t;
    if (!s_prov_open) return;
    char text[40];
    snprintf(text, sizeof(text), "热点 %s", wifi_prov_ap_ssid());
    lv_label_set_text(s_prov_ap, text);
    lv_obj_set_style_text_color(s_prov_ap, lv_color_hex(COLOR_ACCENT), 0);

    const char *state_text;
    uint32_t color;
    switch (wifi_prov_state()) {
        case WIFI_PROV_SAVED:
            state_text = "已保存 · 重启中";
            color = 0x97C459;
            break;
        case WIFI_PROV_WAITING:
            state_text = "等待手机提交";
            color = COLOR_SUBTLE;
            break;
        default:
            state_text = "配网未启动";
            color = 0xE24B4B;
            break;
    }
    lv_label_set_text(s_prov_state, state_text);
    lv_obj_set_style_text_color(s_prov_state, lv_color_hex(color), 0);
}

// 以下两个函数要求调用方已持 LVGL 锁。
static void prov_show(bool started) {
    s_prov_open = true;
    lv_obj_clear_flag(s_prov_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_prov_panel);
    if (started) {
        prov_tick(NULL);
    } else {
        lv_label_set_text(s_prov_ap, "热点启动失败");
        lv_obj_set_style_text_color(s_prov_ap, lv_color_hex(0xE24B4B), 0);
        lv_label_set_text(s_prov_state, "配网未启动");
        lv_obj_set_style_text_color(s_prov_state, lv_color_hex(0xE24B4B), 0);
    }
    if (s_prov_timer == NULL) s_prov_timer = lv_timer_create(prov_tick, 1000, NULL);
}

static void prov_hide(void) {
    s_prov_open = false;
    lv_obj_add_flag(s_prov_panel, LV_OBJ_FLAG_HIDDEN);
    if (s_prov_timer) {
        lv_timer_delete(s_prov_timer);
        s_prov_timer = NULL;
    }
    page_switch(s_state.page);   // 回到进配网前的页面
}

// 长按上键:进/出配网。切 WiFi 模式较重,放在 LVGL 锁外面做。
static void prov_toggle(void) {
    if (s_prov_open) {
        wifi_prov_stop();
        if (!bsp_lvgl_lock(500)) return;
        prov_hide();
        bsp_lvgl_unlock();
        return;
    }
    bool started = (wifi_prov_start() == ESP_OK);
    if (!bsp_lvgl_lock(500)) return;
    prov_show(started);
    bsp_lvgl_unlock();
}

// 事件派发任务上下文调用;UI 操作自行持锁。
static void handle(lucky_input_t in) {
    // 配网页是覆盖全屏的独立层:只认「OK 长按退出」,其余键不进玩法状态机。
    if (s_prov_open) {
        if (in == LUCKY_KEY_OK_LONG) prov_toggle();
        return;
    }
    bool need_draw = false;
    // 开奖页翻页需要知道缓存了几期,由 UI 侧同步进状态机(按当前看的开奖来源取)。
    s_state.issue_count = (s_state.page == LUCKY_PAGE_KL8) ? kl8_result_count() : ssq_result_count();
    if (!lucky_handle_input(&s_state, in, &need_draw)) return;
    if (!bsp_lvgl_lock(500)) return;
    if (need_draw) {
        draw_and_show();
        page_switch(LUCKY_PAGE_RESULT);
    } else {
        page_switch(s_state.page);
    }
    bsp_lvgl_unlock();

    if (s_state.refresh_requested) {
        s_state.refresh_requested = false;
        // 进了哪个开奖页就拉哪份数据;快乐8 只在用到时才拉,省内存也省电。
        if (s_state.page == LUCKY_PAGE_KL8) {
            kl8_result_request();
        } else {
            ssq_result_request();
        }
    }
}

void app_lucky_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    // 容忍 BSP_BTN_RELEASE 等额外事件:不是映射内的组合直接丢弃。
    if (ev == BSP_BTN_PRESS || ev == BSP_BTN_DOUBLE || ev == BSP_BTN_RELEASE) return;
    lucky_input_t in;
    if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP) in = LUCKY_KEY_UP_CLICK;
        else if (btn == BSP_BTN_DOWN) in = LUCKY_KEY_DOWN_CLICK;
        else if (btn == BSP_BTN_OK) in = LUCKY_KEY_OK_CLICK;
        else return;
    } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_OK) {
        in = LUCKY_KEY_OK_LONG;
    } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_DOWN) {
        in = LUCKY_KEY_DOWN_LONG;
    } else if (ev == BSP_BTN_LONG && btn == BSP_BTN_UP) {
        // 上键长按不进状态机:直接开关配网页。
        prov_toggle();
        return;
    } else {
        return;
    }
    handle(in);
}

// 拉取任务上下文回调:数据变化后拿 LVGL 锁,刷新开奖页。
static void on_ssq_update(void) {
    if (!bsp_lvgl_lock(1000)) return;
    if (s_state.page == LUCKY_PAGE_ISSUES) {
        s_state.issue_count = ssq_result_count();
        issues_render();
    }
    bsp_lvgl_unlock();
}

// 快乐8 数据到达:只在快乐8开奖页时重绘,避免无谓的整页刷新。
static void on_kl8_update(void) {
    if (!bsp_lvgl_lock(1000)) return;
    if (s_state.page == LUCKY_PAGE_KL8) {
        s_state.issue_count = kl8_result_count();
        kl8_render();
    }
    bsp_lvgl_unlock();
}

void app_lucky_init(void) {
    s_state.page = LUCKY_PAGE_SETUP;
    s_state.mode = LUCKY_MODE_LUCKY;
    s_state.count = 6;
    s_state.nums_len = 0;
    s_state.issue_idx = 0;
    s_state.issue_count = 0;
    s_state.refresh_requested = false;

    s_scr = lv_obj_create(NULL);
    // 深蓝靛垂直渐变,两页共用,比纯色更有层次。
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x1B2D4F), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(s_scr, lv_color_hex(0x0E1A30), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(s_scr, LV_GRAD_DIR_VER, LV_PART_MAIN);

    s_battery_label = make_label(s_scr, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_battery_label, "-- %");
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_RIGHT, -12, 10);

    // ---- 设置页 ----
    s_setup_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_setup_panel);
    lv_obj_set_size(s_setup_panel, 240, 320);
    lv_obj_clear_flag(s_setup_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 角落装饰球:低透明度大圆,给纯文字页加氛围。
    static const struct { int x, y, r; uint32_t color; } deco[] = {
        {-36, 258, 52, 0x2E86DE},   // 左下,蓝
        {214, 88, 40, 0x7F77DD},    // 右上,紫
    };
    for (int i = 0; i < 2; i++) {
        lv_obj_t *orb = lv_obj_create(s_setup_panel);
        lv_obj_remove_style_all(orb);
        lv_obj_set_size(orb, deco[i].r * 2, deco[i].r * 2);
        lv_obj_set_style_radius(orb, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(orb, LV_OPA_20, LV_PART_MAIN);
        lv_obj_set_style_bg_color(orb, lv_color_hex(deco[i].color), LV_PART_MAIN);
        lv_obj_set_pos(orb, deco[i].x - deco[i].r, deco[i].y - deco[i].r);
    }

    // 标题上方六色圆点条,呼应号码球调色板。
    for (int i = 0; i < BALL_COLORS; i++) {
        lv_obj_t *dot = lv_obj_create(s_setup_panel);
        lv_obj_remove_style_all(dot);
        lv_obj_set_size(dot, 10, 10);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(dot, lv_color_hex(BALL_PALETTE[i]), LV_PART_MAIN);
        lv_obj_align(dot, LV_ALIGN_TOP_MID, i * 18 - 50, 24);
    }

    s_hint_pick = make_label(s_setup_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_hint_pick, "选择号码个数");
    lv_obj_align(s_hint_pick, LV_ALIGN_TOP_MID, 0, 90);

    s_arrow_up = make_label(s_setup_panel, &lucky_font_16, 0xEF9F27);
    lv_label_set_text(s_arrow_up, "▲");
    lv_obj_align(s_arrow_up, LV_ALIGN_TOP_MID, 0, 116);

    // 个数大彩球:颜色随个数轮换,带同色光晕。
    s_count_ball = lv_obj_create(s_setup_panel);
    lv_obj_remove_style_all(s_count_ball);
    lv_obj_set_size(s_count_ball, 88, 88);
    lv_obj_set_style_radius(s_count_ball, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_count_ball, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_align(s_count_ball, LV_ALIGN_TOP_MID, 0, 142);
    s_count_label = make_label(s_count_ball, &lucky_font_40, 0xFFFFFF);
    lv_label_set_text(s_count_label, "6");
    lv_obj_center(s_count_label);

    s_arrow_down = make_label(s_setup_panel, &lucky_font_16, 0x1D9E75);
    lv_label_set_text(s_arrow_down, "▼");
    lv_obj_align(s_arrow_down, LV_ALIGN_TOP_MID, 0, 236);

    // 底部提示胶囊:半透明蓝底,视觉收尾。
    lv_obj_t *hint_pill = lv_obj_create(s_setup_panel);
    lv_obj_remove_style_all(hint_pill);
    lv_obj_set_size(hint_pill, 210, 34);
    lv_obj_set_style_radius(hint_pill, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(hint_pill, LV_OPA_30, LV_PART_MAIN);
    lv_obj_set_style_bg_color(hint_pill, lv_color_hex(0x2E86DE), LV_PART_MAIN);
    lv_obj_align(hint_pill, LV_ALIGN_TOP_MID, 0, 264);
    lv_obj_t *hint_keys = make_label(hint_pill, &lucky_font_16, COLOR_TEXT);
    lv_label_set_text(hint_keys, "确定开始 · 长按下键看开奖");
    lv_obj_center(hint_keys);
    s_hint_keys = hint_keys;

    // 第二行小字:换玩法靠长按 OK,和看开奖的下键分开写,避免胶囊挤成一团。
    s_hint_mode = make_label(s_setup_panel, &lucky_font_16, COLOR_TAB_DIM);
    lv_label_set_text(s_hint_mode, "长按 OK 切换玩法");
    lv_obj_align(s_hint_mode, LV_ALIGN_TOP_MID, 0, 300);

    // ---- 结果页 ----
    s_result_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_result_panel);
    lv_obj_set_size(s_result_panel, 240, 320);
    lv_obj_clear_flag(s_result_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_count_summary = make_label(s_result_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_count_summary, "本组 0 个号码");
    lv_obj_align(s_count_summary, LV_ALIGN_TOP_MID, 0, 28);

    for (int i = 0; i < LUCKY_COUNT_MAX; i++) {
        lv_obj_t *ball = lv_obj_create(s_result_panel);
        lv_obj_remove_style_all(ball);
        lv_obj_set_size(ball, 52, 52);
        lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ball, lv_color_hex(BALL_PALETTE[i % BALL_COLORS]), LV_PART_MAIN);
        lv_obj_set_style_shadow_width(ball, 0, LV_PART_MAIN);
        lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *num = make_label(ball, &lucky_font_24, 0xFFFFFF);
        lv_label_set_text(num, "");
        lv_obj_center(num);
        lv_obj_set_user_data(ball, num);
        s_balls[i] = ball;
    }

    lv_obj_t *hint_again = make_label(s_result_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(hint_again, "确定再抽 · 长按返回");
    lv_obj_align(hint_again, LV_ALIGN_TOP_MID, 0, 278);

    // ---- 开奖页 ----
    s_issues_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_issues_panel);
    lv_obj_set_size(s_issues_panel, 240, 320);
    lv_obj_clear_flag(s_issues_panel, LV_OBJ_FLAG_SCROLLABLE);

    // 层级标签:开奖是双色球的子页,标题行直接写清归属,避免被当成第三玩法。
    lv_obj_t *s_issue_kicker = make_label(s_issues_panel, &lucky_font_16, COLOR_SSQ_RED);
    lv_label_set_text(s_issue_kicker, "双色球开奖");
    lv_obj_align(s_issue_kicker, LV_ALIGN_TOP_MID, 0, 78);

    s_issue_code = make_label(s_issues_panel, &lucky_font_24, COLOR_TEXT);
    lv_label_set_text(s_issue_code, "");
    lv_obj_align(s_issue_code, LV_ALIGN_TOP_MID, 0, 98);

    s_issue_date = make_label(s_issues_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_issue_date, "");
    lv_obj_align(s_issue_date, LV_ALIGN_TOP_MID, 0, 126);

    // 无数据时的提示(加载中/未联网/失败),垂直居中在球区。
    s_issue_status = make_label(s_issues_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_issue_status, "");
    lv_obj_align(s_issue_status, LV_ALIGN_TOP_MID, 0, 186);
    // 离线状态是两行(未配置网络 / 长按上键配网),居中对齐才不歪。
    lv_obj_set_style_text_align(s_issue_status, LV_TEXT_ALIGN_CENTER, 0);

    for (int i = 0; i < LUCKY_SSQ_TOTAL; i++) {
        lv_obj_t *ball = lv_obj_create(s_issues_panel);
        lv_obj_remove_style_all(ball);
        lv_obj_set_size(ball, 44, 44);
        lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ball, lv_color_hex(COLOR_SSQ_RED), LV_PART_MAIN);
        lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *num = make_label(ball, &lucky_font_24, 0xFFFFFF);
        lv_label_set_text(num, "");
        lv_obj_center(num);
        lv_obj_set_user_data(ball, num);
        s_issue_balls[i] = ball;
    }

    lv_obj_t *hint_issue = make_label(s_issues_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(hint_issue, "上下翻期 · 长按返回");
    lv_obj_align(hint_issue, LV_ALIGN_TOP_MID, 0, 298);

    lv_obj_add_flag(s_issues_panel, LV_OBJ_FLAG_HIDDEN);

    // ---- 快乐8开奖页(快乐选号的官方开奖) ----
    s_kl8_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_kl8_panel);
    lv_obj_set_size(s_kl8_panel, 240, 320);
    lv_obj_clear_flag(s_kl8_panel, LV_OBJ_FLAG_SCROLLABLE);

    s_kl8_kicker = make_label(s_kl8_panel, &lucky_font_16, COLOR_SSQ_RED);
    lv_label_set_text(s_kl8_kicker, "快乐8开奖");
    lv_obj_align(s_kl8_kicker, LV_ALIGN_TOP_MID, 0, 78);

    s_kl8_code = make_label(s_kl8_panel, &lucky_font_24, COLOR_TEXT);
    lv_label_set_text(s_kl8_code, "");
    lv_obj_align(s_kl8_code, LV_ALIGN_TOP_MID, 0, 98);

    s_kl8_date = make_label(s_kl8_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_kl8_date, "");
    lv_obj_align(s_kl8_date, LV_ALIGN_TOP_MID, 0, 126);

    s_kl8_status = make_label(s_kl8_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_kl8_status, "");
    lv_obj_align(s_kl8_status, LV_ALIGN_TOP_MID, 0, 200);
    lv_obj_set_style_text_align(s_kl8_status, LV_TEXT_ALIGN_CENTER, 0);

    for (int i = 0; i < KL8_NUM_COUNT; i++) {
        lv_obj_t *ball = lv_obj_create(s_kl8_panel);
        lv_obj_remove_style_all(ball);
        lv_obj_set_size(ball, 30, 30);
        lv_obj_set_style_radius(ball, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(ball, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(ball, lv_color_hex(COLOR_ACCENT), LV_PART_MAIN);
        lv_obj_add_flag(ball, LV_OBJ_FLAG_HIDDEN);
        lv_obj_t *num = make_label(ball, &lucky_font_16, 0xFFFFFF);
        lv_label_set_text(num, "");
        lv_obj_center(num);
        lv_obj_set_user_data(ball, num);
        s_kl8_balls[i] = ball;
    }

    lv_obj_t *hint_kl8 = make_label(s_kl8_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(hint_kl8, "上下翻期 · 长按返回");
    lv_obj_align(hint_kl8, LV_ALIGN_TOP_MID, 0, 298);

    lv_obj_add_flag(s_kl8_panel, LV_OBJ_FLAG_HIDDEN);

    // ---- 两个主玩法标签页(屏幕级,设置页与开奖页共用) ----
    s_tabs = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_tabs);
    lv_obj_set_size(s_tabs, 240, 60);
    lv_obj_clear_flag(s_tabs, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(s_tabs, LV_ALIGN_TOP_MID, 0, 30);

    for (int i = 0; i < 2; i++) {
        s_tab[i] = make_label(s_tabs, &lucky_font_20, COLOR_TAB_DIM);
        lv_label_set_text(s_tab[i], TAB_TEXT[i]);
        // 标签中心对齐父容器中心 + 相对偏移;容器高 60,文字放顶部。
        lv_obj_align(s_tab[i], LV_ALIGN_TOP_MID, TAB_CX[i] - 120, 6);

        s_tab_bar[i] = lv_obj_create(s_tabs);
        lv_obj_remove_style_all(s_tab_bar[i]);
        lv_obj_set_size(s_tab_bar[i], TAB_BAR_W[i], 4);
        lv_obj_set_style_radius(s_tab_bar[i], 2, LV_PART_MAIN);
        lv_obj_set_style_bg_color(s_tab_bar[i], lv_color_hex(TAB_BAR_COLOR[i]), LV_PART_MAIN);
        lv_obj_align(s_tab_bar[i], LV_ALIGN_TOP_MID, TAB_CX[i] - 120, 40);
        lv_obj_add_flag(s_tab_bar[i], LV_OBJ_FLAG_HIDDEN);
    }

    // ---- 配网页(最后创建,盖在所有页面之上) ----
    s_prov_panel = lv_obj_create(s_scr);
    lv_obj_remove_style_all(s_prov_panel);
    lv_obj_set_size(s_prov_panel, 240, 320);
    lv_obj_set_style_bg_opa(s_prov_panel, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_prov_panel, lv_color_hex(0x0E1A30), LV_PART_MAIN);
    lv_obj_clear_flag(s_prov_panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *prov_title = make_label(s_prov_panel, &lucky_font_20, COLOR_TEXT);
    lv_label_set_text(prov_title, "配网模式");
    lv_obj_align(prov_title, LV_ALIGN_TOP_MID, 0, 16);

    // 三步式引导:首次上手的人只看到「192.168.4.1」不知道要先连热点,
    // 所以把「连哪个热点 / 用什么打开 / 填什么」按序号写全。
    lv_obj_t *prov_s1 = make_label(s_prov_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(prov_s1, "① 手机连这个热点");
    lv_obj_align(prov_s1, LV_ALIGN_TOP_MID, 0, 50);

    s_prov_ap = make_label(s_prov_panel, &lucky_font_20, COLOR_ACCENT);
    lv_label_set_text(s_prov_ap, "FoloToy-0000");
    lv_obj_align(s_prov_ap, LV_ALIGN_TOP_MID, 0, 72);

    lv_obj_t *prov_s2 = make_label(s_prov_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(prov_s2, "② 手机浏览器打开");
    lv_obj_align(prov_s2, LV_ALIGN_TOP_MID, 0, 106);

    lv_obj_t *prov_url = make_label(s_prov_panel, &lucky_font_24, COLOR_TEXT);
    lv_label_set_text(prov_url, "192.168.4.1");
    lv_obj_align(prov_url, LV_ALIGN_TOP_MID, 0, 128);

    lv_obj_t *prov_s3 = make_label(s_prov_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(prov_s3, "③ 选家里 WiFi 填密码");
    lv_obj_align(prov_s3, LV_ALIGN_TOP_MID, 0, 168);

    lv_obj_t *prov_note = make_label(s_prov_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(prov_note, "保存后自动重启联网");
    lv_obj_align(prov_note, LV_ALIGN_TOP_MID, 0, 190);

    s_prov_state = make_label(s_prov_panel, &lucky_font_16, COLOR_SUBTLE);
    lv_label_set_text(s_prov_state, "等待手机提交");
    lv_obj_align(s_prov_state, LV_ALIGN_TOP_MID, 0, 228);

    // 2.4G 限制写在屏幕上:社区用户最容易卡在这里(5G 连不上还以为设备坏了)。
    lv_obj_t *prov_band = make_label(s_prov_panel, &lucky_font_16, COLOR_WARN);
    lv_label_set_text(prov_band, "仅支持 2.4G 网络");
    lv_obj_align(prov_band, LV_ALIGN_TOP_MID, 0, 262);

    lv_obj_t *prov_hint = make_label(s_prov_panel, &lucky_font_16, COLOR_TAB_DIM);
    lv_label_set_text(prov_hint, "长按 OK 退出");
    lv_obj_align(prov_hint, LV_ALIGN_TOP_MID, 0, 294);

    lv_obj_add_flag(s_prov_panel, LV_OBJ_FLAG_HIDDEN);

    page_switch(LUCKY_PAGE_SETUP);

    battery_tick(NULL);                       // 立即显示一次,不用等刷新周期
    s_battery_timer = lv_timer_create(battery_tick, 30000, NULL);

    lv_screen_load(s_scr);

    // 开奖数据:先建拉取任务并请求一次;联网由 main 在初始化后启动。
    ssq_result_init(on_ssq_update);
    ssq_result_request();
    // 快乐8 数据按需拉取:第一次进快乐8开奖页时才请求,不在开机时抢内存。
    kl8_result_init(on_kl8_update);
}

// 开机没有可用凭据时自动进配网:否则用户拿着设备根本无从下手。
// 由 main 在 net_wifi_start() 之后调用(此时才知道 NVS 里有没有凭据)。
void app_lucky_auto_prov(void) {
    if (net_wifi_configured()) return;
    ESP_LOGW(TAG, "没有 WiFi 凭据,自动进入配网");
    bool started = (wifi_prov_start() == ESP_OK);
    if (!bsp_lvgl_lock(1000)) return;
    prov_show(started);
    bsp_lvgl_unlock();
}
