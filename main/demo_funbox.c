#include "demo.h"

#include <stdio.h>
#include <string.h>

#include "bsp_battery.h"
#include "bsp_display.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "funbox_audio.h"
#include "funbox_model.h"
#include "lvgl.h"

#define COLOR_CREAM  0xFFF7E8
#define COLOR_INK    0x443A55
#define COLOR_PINK   0xFF7FA8
#define COLOR_YELLOW 0xFFD75A
#define COLOR_MINT   0x83DFC1
#define COLOR_BLUE   0x82C8F8
#define COLOR_PURPLE 0xBCA4FF
#define COLOR_WHITE  0xFFFFFF

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} funbox_input_t;

typedef struct {
    const char *title;
    const char *description;
    const char *mark;
    uint32_t color;
} funbox_app_info_t;

static const funbox_app_info_t APPS[FUNBOX_MENU_COUNT] = {
    { "答案之书", "迷茫的时候，按一按", "?", COLOR_PURPLE },
    { "随机起名", "送给新生宝宝一个好名字", "Aa", COLOR_MINT },
    { "随机短语", "今天的个性签名写什么？", "\"\"", COLOR_BLUE },
    { "求签", "给今天抽一支好运签", "LUCK", COLOR_YELLOW },
    { "滚动骰子", "选择困难时，让骰子决定", "6", COLOR_PINK },
    { "出行推荐", "下一站去哪里走走？", "GO", COLOR_BLUE },
    { "美食推荐", "这一顿吃什么？", "YUM", COLOR_YELLOW },
    { "影视推荐", "今晚看点什么？", "PLAY", COLOR_PURPLE },
    { "玩什么", "随机解锁一种快乐", "FUN", COLOR_MINT },
    { "干什么", "无聊时领一个小任务", "DO", COLOR_PINK },
    { "告白彩蛋", "长按确认键也能随时抵达", "LOVE", COLOR_PINK },
    { "设置", "声音、背景音乐和屏幕亮度", "SET", COLOR_BLUE },
};

static const char *const ANSWERS[] = {
    "去做吧，风会站在你这边。", "先睡一觉，答案会更清楚。",
    "相信第一次心动的选择。", "再等一等，好事正在靠近。",
    "今天适合勇敢一点。", "换条路，会看到新风景。",
    "问问那个最懂你的朋友。", "答案其实已经在你心里。",
};
static const char *const PHRASES[] = {
    "把普通日子过成闪光限定。", "风有约，花不误。",
    "今天也在认真收集快乐。", "慢慢来，比较快。",
    "生活边角料也会开花。", "保持可爱，万事顺意。",
    "山高路远，看世界也找自己。", "好运正在派送中。",
};
static const char *const FORTUNES[] = {
    "上上签｜心想事成", "上吉签｜贵人相助", "中吉签｜稳中有喜",
    "小吉签｜转角遇好运", "平安签｜慢慢来更好", "蓄力签｜明天会发光",
};
static const char *const TRAVEL[] = {
    "去公园追一场落日", "逛一间没去过的书店", "坐地铁到陌生的一站",
    "找一条适合散步的老街", "去植物园吸一口绿意", "来一次城市骑行",
    "带杯饮料去江边吹风", "去博物馆认识旧时光",
};
static const char *const FOOD[] = {
    "热乎乎的番茄牛腩", "一碗香喷喷的拌面", "脆脆的炸鸡配气泡水",
    "暖胃的小火锅", "寿司和一份玉子烧", "咖喱饭加一颗溏心蛋",
    "清爽的越南河粉", "今天奖励自己吃甜品",
};
static const char *const MOVIES[] = {
    "看一部轻松治愈的动画", "选一部九十分钟喜剧", "重温最喜欢的科幻片",
    "找一部高分悬疑电影", "看一集自然纪录片", "随机点开一部老电影",
    "挑一部音乐题材作品", "和朋友看一部冒险片",
};
static const char *const PLAY[] = {
    "来一局俄罗斯方块", "画一张三分钟涂鸦", "玩一轮你画我猜",
    "搭一个纸牌小城堡", "拍十张同色系照片", "和朋友玩词语接龙",
    "拼一小块拼图", "学一个简单的魔术",
};
static const char *const DO[] = {
    "整理桌面五分钟", "给很久没联系的人问声好", "喝一杯水并伸个懒腰",
    "记录今天的三个小确幸", "出门走够一千步", "给未来的自己写句话",
    "清理相册里的十张废片", "认真听完一首歌",
};

static const char *TAG = "funbox";
static funbox_model_t s_model;
static lv_obj_t *s_screen;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_volume_label;
static lv_timer_t *s_ui_timer;
static QueueHandle_t s_input_queue;
static TaskHandle_t s_battery_task;
static volatile bool s_battery_active;
static volatile bool s_battery_idle = true;
static volatile int s_battery_soc = -1;
static uint32_t s_rendered_generation = UINT32_MAX;
static uint64_t s_ok_pressed_at_ms;
static bool s_love_shortcut_fired;
static char s_result[160];

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static lv_obj_t *box(lv_obj_t *parent, int x, int y, int width, int height,
                     uint32_t color, int radius)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(object, 0, 0);
    lv_obj_set_style_radius(object, radius, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    return object;
}

static lv_obj_t *label(lv_obj_t *parent, const char *text, int x, int y,
                       int width, uint32_t color, const lv_font_t *font)
{
    lv_obj_t *object = lv_label_create(parent);
    lv_label_set_text(object, text);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_width(object, width);
    lv_label_set_long_mode(object, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_text_font(object, font, 0);
    lv_obj_set_style_text_align(object, LV_TEXT_ALIGN_CENTER, 0);
    return object;
}

static void add_dots(lv_obj_t *parent)
{
    static const uint32_t colors[] = { COLOR_PINK, COLOR_YELLOW, COLOR_MINT, COLOR_BLUE };
    for (int i = 0; i < 8; i++) {
        int size = 4 + (i % 3) * 2;
        box(parent, 8 + (i * 37) % 220, 40 + (i * 53) % 250,
            size, size, colors[i % 4], size / 2);
    }
}

static void add_top_bar(lv_obj_t *screen)
{
    box(screen, 0, 0, 240, 34, COLOR_WHITE, 0);
    s_volume_label = label(screen, "", 7, 9, 75, COLOR_INK, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(s_volume_label, LV_TEXT_ALIGN_LEFT, 0);
    s_battery_label = label(screen, "", 164, 9, 68, COLOR_INK, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(s_battery_label, LV_TEXT_ALIGN_RIGHT, 0);
}

static void refresh_status(void)
{
    if (s_volume_label) {
        lv_label_set_text_fmt(s_volume_label, "VOL %u", funbox_model_volume(&s_model));
    }
    if (s_battery_label) {
        if (s_battery_soc >= 0) lv_label_set_text_fmt(s_battery_label, "BAT %d%%", s_battery_soc);
        else lv_label_set_text(s_battery_label, "BAT --");
    }
}

static lv_obj_t *create_screen(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_CREAM), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);
    add_dots(screen);
    add_top_bar(screen);
    return screen;
}

static void add_character(lv_obj_t *parent, uint32_t color, const char *mark,
                          int center_x, int top_y)
{
    box(parent, center_x - 40, top_y + 6, 80, 70, COLOR_INK, 34);
    box(parent, center_x - 38, top_y + 3, 76, 69, color, 32);
    box(parent, center_x - 21, top_y + 25, 7, 9, COLOR_INK, 4);
    box(parent, center_x + 14, top_y + 25, 7, 9, COLOR_INK, 4);
    box(parent, center_x - 8, top_y + 43, 16, 4, COLOR_INK, 2);
    box(parent, center_x - 47, top_y + 42, 12, 7, color, 4);
    box(parent, center_x + 35, top_y + 42, 12, 7, color, 4);
    lv_obj_t *badge = box(parent, center_x - 25, top_y - 11, 50, 25, COLOR_WHITE, 12);
    lv_obj_set_style_border_color(badge, lv_color_hex(COLOR_INK), 0);
    lv_obj_set_style_border_width(badge, 2, 0);
    lv_obj_t *mark_label = label(badge, mark, 2, 5, 46, COLOR_INK, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(mark_label, LV_TEXT_ALIGN_CENTER, 0);
}

static void build_home(lv_obj_t *screen)
{
    const funbox_app_info_t *app = &APPS[s_model.selected];
    label(screen, "趣玩盒子", 20, 43, 200, COLOR_INK,
          &lv_font_source_han_sans_sc_14_cjk);
    label(screen, "11种玩法 + 设置", 20, 63, 200, 0x8C8098,
          &lv_font_source_han_sans_sc_14_cjk);

    box(screen, 16, 91, 212, 170, COLOR_INK, 26);
    lv_obj_t *card = box(screen, 13, 87, 212, 170, app->color, 26);
    box(card, 12, 12, 188, 146, COLOR_WHITE, 20);
    add_character(card, app->color, app->mark, 106, 35);
    label(card, app->title, 15, 111, 182, COLOR_INK,
          &lv_font_source_han_sans_sc_14_cjk);
    label(card, app->description, 18, 134, 176, 0x756B80,
          &lv_font_source_han_sans_sc_14_cjk);

    label(screen, "UP 上一个   OK 进入   DN 下一个", 8, 276, 224, COLOR_INK,
          &lv_font_source_han_sans_sc_14_cjk);
    if (s_model.selected == FUNBOX_PLAY_COUNT) {
        label(screen, "SET", 95, 298, 50, 0x8C8098, &lv_font_montserrat_14);
    } else {
        lv_obj_t *counter = label(screen, "", 80, 298, 80, 0x8C8098,
                                  &lv_font_montserrat_14);
        lv_label_set_text_fmt(counter, "%02u / %02u", s_model.selected + 1, FUNBOX_PLAY_COUNT);
    }
}

static void prepare_result(void)
{
    if (s_rendered_generation == s_model.generation) return;
    s_rendered_generation = s_model.generation;
    uint32_t index;
    switch (s_model.current_app) {
        case 0:
            index = funbox_model_random(&s_model, sizeof(ANSWERS) / sizeof(ANSWERS[0]));
            snprintf(s_result, sizeof(s_result), "%s", ANSWERS[index]);
            break;
        case 1: {
            static const char *const surnames[] = { "林", "陈", "苏", "江", "顾", "叶", "沈", "夏" };
            static const char *const given[3][8] = {
                { "小满", "乐柠", "团团", "星禾", "安安", "可心", "米粒", "多多" },
                { "知远", "清和", "景行", "云舒", "嘉木", "望舒", "言蹊", "若川" },
                { "星野", "蓝岚", "光屿", "弦月", "云舟", "初晓", "森语", "霁光" },
            };
            uint32_t surname = funbox_model_random(&s_model, 8);
            uint32_t name = funbox_model_random(&s_model, 8);
            snprintf(s_result, sizeof(s_result), "%s%s", surnames[surname], given[s_model.option][name]);
            break;
        }
        case 2:
            index = funbox_model_random(&s_model, sizeof(PHRASES) / sizeof(PHRASES[0]));
            snprintf(s_result, sizeof(s_result), "%s", PHRASES[index]);
            break;
        case 3:
            index = funbox_model_random(&s_model, sizeof(FORTUNES) / sizeof(FORTUNES[0]));
            snprintf(s_result, sizeof(s_result), "%s", FORTUNES[index]);
            break;
        case 4: {
            int count = s_model.option + 1;
            int total = 0;
            int used = 0;
            for (int i = 0; i < count; i++) {
                int value = (int)funbox_model_random(&s_model, 6) + 1;
                total += value;
                used += snprintf(s_result + used, sizeof(s_result) - (size_t)used,
                                 i ? " + %d" : "%d", value);
            }
            snprintf(s_result + used, sizeof(s_result) - (size_t)used, "  =  %d", total);
            break;
        }
        case 5:
            index = funbox_model_random(&s_model, sizeof(TRAVEL) / sizeof(TRAVEL[0]));
            snprintf(s_result, sizeof(s_result), "%s", TRAVEL[index]);
            break;
        case 6:
            index = funbox_model_random(&s_model, sizeof(FOOD) / sizeof(FOOD[0]));
            snprintf(s_result, sizeof(s_result), "%s", FOOD[index]);
            break;
        case 7:
            index = funbox_model_random(&s_model, sizeof(MOVIES) / sizeof(MOVIES[0]));
            snprintf(s_result, sizeof(s_result), "%s", MOVIES[index]);
            break;
        case 8:
            index = funbox_model_random(&s_model, sizeof(PLAY) / sizeof(PLAY[0]));
            snprintf(s_result, sizeof(s_result), "%s", PLAY[index]);
            break;
        case 9:
            index = funbox_model_random(&s_model, sizeof(DO) / sizeof(DO[0]));
            snprintf(s_result, sizeof(s_result), "%s", DO[index]);
            break;
        default:
            snprintf(s_result, sizeof(s_result), "我喜欢你");
            break;
    }
}

static void build_app(lv_obj_t *screen)
{
    const funbox_app_info_t *app = &APPS[s_model.current_app];
    prepare_result();
    label(screen, app->title, 20, 43, 200, COLOR_INK,
          &lv_font_source_han_sans_sc_14_cjk);
    add_character(screen, app->color, app->mark, 120, 79);

    box(screen, 16, 169, 212, 92, COLOR_INK, 20);
    lv_obj_t *result = box(screen, 13, 165, 212, 92, COLOR_WHITE, 20);
    lv_obj_t *result_label = label(result, s_result, 15, 25, 182, COLOR_INK,
                                   &lv_font_source_han_sans_sc_14_cjk);
    lv_obj_set_style_text_line_space(result_label, 5, 0);

    if (s_model.current_app == 1) {
        static const char *const styles[] = { "可爱风", "古典风", "未来风" };
        label(screen, styles[s_model.option], 60, 266, 120, 0x756B80,
              &lv_font_source_han_sans_sc_14_cjk);
    } else if (s_model.current_app == 4) {
        lv_obj_t *dice_count = label(screen, "", 60, 266, 120, 0x756B80,
                                     &lv_font_source_han_sans_sc_14_cjk);
        lv_label_set_text_fmt(dice_count, "%u 个骰子", s_model.option + 1);
    }
    label(screen, "UP/DN 选择   OK 再来   双击OK 返回", 5, 291, 230, COLOR_INK,
          &lv_font_source_han_sans_sc_14_cjk);
}

static void add_heart(lv_obj_t *parent, int x, int y, int size, uint32_t color)
{
    box(parent, x, y + size / 4, size, size * 3 / 4, color, size / 3);
    box(parent, x, y, size / 2 + 2, size / 2 + 2, color, size / 2);
    box(parent, x + size / 2 - 2, y, size / 2 + 2, size / 2 + 2, color, size / 2);
}

static void build_love(lv_obj_t *screen)
{
    add_heart(screen, 18, 56, 26, COLOR_PINK);
    add_heart(screen, 194, 62, 20, COLOR_YELLOW);
    add_heart(screen, 31, 238, 18, COLOR_PURPLE);
    add_heart(screen, 187, 232, 28, COLOR_PINK);
    box(screen, 19, 88, 202, 139, COLOR_INK, 30);
    lv_obj_t *card = box(screen, 15, 84, 202, 139, 0xFFE0E9, 30);
    add_character(card, COLOR_PINK, "LOVE", 101, 23);
    label(card, "我喜欢你", 15, 99, 172, 0xD94275,
          &lv_font_source_han_sans_sc_14_cjk);
    label(card, "每一次心动，都值得被认真收藏。", 18, 121, 166, COLOR_INK,
          &lv_font_source_han_sans_sc_14_cjk);
    label(screen, "长按OK 随时进入   双击OK 返回", 10, 283, 220, COLOR_INK,
          &lv_font_source_han_sans_sc_14_cjk);
}

static void build_settings(lv_obj_t *screen)
{
    label(screen, "设置", 20, 43, 200, COLOR_INK,
          &lv_font_source_han_sans_sc_14_cjk);
    static const char *const names[] = { "音量", "背景音乐", "屏幕亮度" };
    for (int row = 0; row < 3; row++) {
        uint32_t color = row == s_model.setting_row ? COLOR_YELLOW : COLOR_WHITE;
        box(screen, 17, 78 + row * 61, 206, 49, COLOR_INK, 16);
        lv_obj_t *panel = box(screen, 14, 75 + row * 61, 206, 49, color, 16);
        lv_obj_t *name = label(panel, names[row], 12, 15, 92, COLOR_INK,
                               &lv_font_source_han_sans_sc_14_cjk);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_LEFT, 0);
        lv_obj_t *value = label(panel, "", 108, 15, 86, COLOR_INK,
                                &lv_font_source_han_sans_sc_14_cjk);
        lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_RIGHT, 0);
        if (row == 0) lv_label_set_text_fmt(value, "%u%%", funbox_model_volume(&s_model));
        if (row == 1) lv_label_set_text(value, s_model.bgm_enabled ? "开启" : "关闭");
        if (row == 2) lv_label_set_text_fmt(value, "%u%%", funbox_model_brightness(&s_model));
    }
    label(screen, "UP/DN 选择   OK 调节   双击OK 返回", 7, 282, 226, COLOR_INK,
          &lv_font_source_han_sans_sc_14_cjk);
    label(screen, "背景音乐为原创轻快芯片旋律", 20, 306, 200, 0x8C8098,
          &lv_font_source_han_sans_sc_14_cjk);
}

static void rebuild_screen(void)
{
    lv_obj_t *old_screen = s_screen;
    s_screen = create_screen();
    if (s_model.page == FUNBOX_PAGE_HOME) build_home(s_screen);
    if (s_model.page == FUNBOX_PAGE_APP) build_app(s_screen);
    if (s_model.page == FUNBOX_PAGE_SETTINGS) build_settings(s_screen);
    if (s_model.page == FUNBOX_PAGE_LOVE) build_love(s_screen);
    refresh_status();
    lv_screen_load(s_screen);
    if (old_screen) lv_obj_delete(old_screen);
}

static void apply_settings_if_changed(uint8_t old_volume, uint8_t old_brightness,
                                      bool old_background)
{
    uint8_t volume = funbox_model_volume(&s_model);
    uint8_t brightness = funbox_model_brightness(&s_model);
    if (volume != old_volume) funbox_audio_set_volume(volume);
    if (brightness != old_brightness) bsp_display_backlight(brightness);
    if (s_model.bgm_enabled != old_background) {
        funbox_audio_set_background(s_model.bgm_enabled);
    }
}

static void handle_input(const funbox_input_t *input)
{
    funbox_action_t action;
    if (input->event == BSP_BTN_PRESS) {
        if (input->button == BSP_BTN_OK) {
            s_ok_pressed_at_ms = now_ms();
            s_love_shortcut_fired = false;
        }
        return;
    }
    if (input->event == BSP_BTN_RELEASE && input->button == BSP_BTN_OK) {
        s_ok_pressed_at_ms = 0;
        return;
    }
    if (input->button == BSP_BTN_OK &&
        (input->event == BSP_BTN_CLICK || input->event == BSP_BTN_DOUBLE)) {
        s_ok_pressed_at_ms = 0;
    }
    if (input->event == BSP_BTN_DOUBLE && input->button == BSP_BTN_OK) {
        action = FUNBOX_ACTION_BACK;
    } else if (input->event == BSP_BTN_CLICK) {
        if (input->button == BSP_BTN_UP) action = FUNBOX_ACTION_PREV;
        else if (input->button == BSP_BTN_DOWN) action = FUNBOX_ACTION_NEXT;
        else action = FUNBOX_ACTION_CONFIRM;
    } else {
        return;
    }

    uint8_t old_volume = funbox_model_volume(&s_model);
    uint8_t old_brightness = funbox_model_brightness(&s_model);
    bool old_background = s_model.bgm_enabled;
    if (!funbox_model_apply(&s_model, action)) return;
    apply_settings_if_changed(old_volume, old_brightness, old_background);
    if (action == FUNBOX_ACTION_CONFIRM && s_model.page != FUNBOX_PAGE_SETTINGS) {
        funbox_audio_reward();
    }
    rebuild_screen();
}

static void ui_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    funbox_input_t input;
    while (s_input_queue && xQueueReceive(s_input_queue, &input, 0) == pdTRUE) {
        handle_input(&input);
    }
    if (s_ok_pressed_at_ms && !s_love_shortcut_fired &&
        now_ms() - s_ok_pressed_at_ms >= 3000U) {
        s_love_shortcut_fired = true;
        funbox_model_apply(&s_model, FUNBOX_ACTION_LOVE);
        funbox_audio_reward();
        rebuild_screen();
    }
    refresh_status();
}

static void battery_task(void *arg)
{
    (void)arg;
    while (s_battery_active) {
        s_battery_idle = false;
        s_battery_soc = bsp_battery_soc();
        s_battery_idle = true;
        for (int i = 0; i < 100 && s_battery_active; i++) vTaskDelay(pdMS_TO_TICKS(100));
    }
    s_battery_idle = true;
    s_battery_task = NULL;
    vTaskDelete(NULL);
}

void demo_funbox_enter(void)
{
    funbox_model_init(&s_model, (uint32_t)esp_timer_get_time());
    s_rendered_generation = UINT32_MAX;
    s_ok_pressed_at_ms = 0;
    s_love_shortcut_fired = false;
    s_input_queue = xQueueCreate(12, sizeof(funbox_input_t));
    if (!s_input_queue) ESP_LOGE(TAG, "input queue allocation failed");

    if (funbox_audio_start(funbox_model_volume(&s_model), s_model.bgm_enabled) != ESP_OK) {
        ESP_LOGW(TAG, "audio unavailable; application remains usable");
    }
    s_battery_active = true;
    if (!s_battery_task &&
        xTaskCreate(battery_task, "funbox_battery", 2048, NULL, 3, &s_battery_task) != pdPASS) {
        s_battery_task = NULL;
        s_battery_active = false;
        ESP_LOGW(TAG, "battery status task unavailable");
    }

    rebuild_screen();
    s_ui_timer = lv_timer_create(ui_timer_cb, 30, NULL);
}

void demo_funbox_exit(void)
{
    s_battery_active = false;
    for (int i = 0; i < 100 && !s_battery_idle; i++) vTaskDelay(pdMS_TO_TICKS(2));
    funbox_audio_stop();
    if (s_ui_timer) {
        lv_timer_delete(s_ui_timer);
        s_ui_timer = NULL;
    }
    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
    }
    if (s_input_queue) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
    }
    s_battery_label = NULL;
    s_volume_label = NULL;
}

void demo_funbox_key(bsp_btn_t button, bsp_btn_ev_t event)
{
    if (event == BSP_BTN_PRESS) funbox_audio_button(button);
    if (!s_input_queue) return;
    funbox_input_t input = { .button = button, .event = event };
    if (xQueueSend(s_input_queue, &input, 0) != pdTRUE) {
        ESP_LOGW(TAG, "input queue full: button=%d event=%d", button, event);
    }
}
