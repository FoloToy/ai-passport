// ColorOS-inspired wooden-fish app for FoloToy AI Passport.
#include <stdint.h>
#include <math.h>

#include "bsp_audio.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#define SCREEN_W              240
#define SCREEN_H              320
#define STRIKER_REST_Y        116
#define STRIKER_HIT_Y         161
#define STRIKER_X             150
#define FLOAT_START_Y         145
#define FLOAT_DISTANCE        42
#define AUDIO_SAMPLE_RATE     16000
#define AUDIO_TONE_MS         140
#define AUDIO_SAMPLE_COUNT    (AUDIO_SAMPLE_RATE * AUDIO_TONE_MS / 1000)

#define COLOR_TEXT            0xFFF1CF
#define COLOR_HANDLE          0xC86D2F
#define COLOR_HANDLE_LIGHT    0xF0A35E
#define COLOR_HEAD            0xF2E2B1
#define COLOR_HEAD_BORDER     0xC49B57

static const char *TAG = "coloros_muyu";

LV_FONT_DECLARE(font_muyu_22);

extern const uint8_t s_muyu_bg_start[]
    asm("_binary_coloros_muyu_bg_rgb565_start");

static const lv_image_dsc_t s_muyu_bg = {
    .header = {
        .magic = LV_IMAGE_HEADER_MAGIC,
        .cf = LV_COLOR_FORMAT_RGB565,
        .flags = 0,
        .w = SCREEN_W,
        .h = SCREEN_H,
        .stride = SCREEN_W * 2,
    },
    .data_size = SCREEN_W * SCREEN_H * 2,
    .data = s_muyu_bg_start,
};

static lv_obj_t *s_screen;
static lv_obj_t *s_count_label;
static lv_obj_t *s_handle;
static lv_obj_t *s_handle_highlight;
static lv_obj_t *s_head;
static lv_obj_t *s_impact[3];
static lv_timer_t *s_impact_timer;
static uint32_t s_count;
static TaskHandle_t s_audio_task;
static int16_t s_wood_tone[AUDIO_SAMPLE_COUNT];

static lv_point_precise_t s_handle_points[2];
static lv_point_precise_t s_highlight_points[2];

static void wood_tone_generate(void)
{
    uint32_t noise_state = 0x4D595955;

    for (int i = 0; i < AUDIO_SAMPLE_COUNT; i++) {
        float t = (float)i / AUDIO_SAMPLE_RATE;
        float envelope = expf(-27.0f * t);
        float body = 0.68f * sinf(6.28318530718f * 720.0f * t)
                   + 0.32f * sinf(6.28318530718f * 1170.0f * t)
                   + 0.16f * sinf(6.28318530718f * 1890.0f * t);
        float attack = 0.0f;

        if (i < AUDIO_SAMPLE_RATE / 100) {
            noise_state = noise_state * 1664525U + 1013904223U;
            float noise = (float)((int32_t)(noise_state >> 16) - 32768) / 32768.0f;
            attack = noise * 0.24f * (1.0f - (float)i / (AUDIO_SAMPLE_RATE / 100));
        }

        float sample = envelope * body + attack;
        if (sample > 1.0f) sample = 1.0f;
        if (sample < -1.0f) sample = -1.0f;
        s_wood_tone[i] = (int16_t)(sample * 10500.0f);
    }
}

static void audio_task(void *argument)
{
    (void)argument;

    for (;;) {
        ulTaskNotifyTake(pdFALSE, portMAX_DELAY);
        bsp_audio_write(s_wood_tone, sizeof(s_wood_tone));
    }
}

static bool audio_start(void)
{
    if (bsp_audio_init() != ESP_OK ||
        bsp_audio_set_format(AUDIO_SAMPLE_RATE, 16, 1) != ESP_OK) {
        ESP_LOGW(TAG, "audio unavailable; continuing without sound");
        return false;
    }

    bsp_audio_set_volume(72);
    wood_tone_generate();
    if (xTaskCreate(audio_task, "muyu_audio", 3072, NULL, 5,
                    &s_audio_task) != pdPASS) {
        ESP_LOGW(TAG, "audio task creation failed");
        s_audio_task = NULL;
        return false;
    }
    return true;
}

static lv_obj_t *label_create(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, &font_muyu_22, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_text_opa(label, LV_OPA_COVER, 0);
    return label;
}

static lv_obj_t *line_create(lv_obj_t *parent, lv_point_precise_t points[2],
                             int width, uint32_t color)
{
    lv_obj_t *line = lv_line_create(parent);
    lv_line_set_points_mutable(line, points, 2);
    lv_obj_set_style_line_width(line, width, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(color), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    return line;
}

static void striker_set_y(void *object, int32_t y)
{
    (void)object;
    int32_t handle_end_y = y - 35;

    s_handle_points[0] = (lv_point_precise_t){ STRIKER_X, y };
    s_handle_points[1] = (lv_point_precise_t){ SCREEN_W + 12, handle_end_y };
    s_highlight_points[0] = (lv_point_precise_t){ STRIKER_X + 5, y - 3 };
    s_highlight_points[1] = (lv_point_precise_t){ SCREEN_W + 12, handle_end_y - 3 };
    lv_line_set_points_mutable(s_handle, s_handle_points, 2);
    lv_line_set_points_mutable(s_handle_highlight, s_highlight_points, 2);
    lv_obj_set_pos(s_head, STRIKER_X - 14, y - 14);
}

static void impact_hide(lv_timer_t *timer)
{
    for (size_t i = 0; i < 3; i++) {
        lv_obj_add_flag(s_impact[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_timer_pause(timer);
}

static void impact_show(void)
{
    for (size_t i = 0; i < 3; i++) {
        lv_obj_remove_flag(s_impact[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_timer_reset(s_impact_timer);
    lv_timer_resume(s_impact_timer);
}

static void float_feedback_set(void *object, int32_t progress)
{
    lv_obj_t *label = object;
    lv_obj_set_y(label, FLOAT_START_Y - progress * FLOAT_DISTANCE / 255);
    lv_obj_set_style_opa(label, (lv_opa_t)(255 - progress), 0);
}

static void float_feedback_done(lv_anim_t *animation)
{
    lv_obj_t *label = lv_anim_get_user_data(animation);
    lv_obj_delete_async(label);
}

static void float_feedback_create(void)
{
    lv_obj_t *label = label_create(s_screen, "功德 + 1");
    lv_obj_set_pos(label, 18, FLOAT_START_Y);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, label);
    lv_anim_set_user_data(&animation, label);
    lv_anim_set_exec_cb(&animation, float_feedback_set);
    lv_anim_set_values(&animation, 0, 255);
    lv_anim_set_duration(&animation, 1050);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_set_completed_cb(&animation, float_feedback_done);
    lv_anim_start(&animation);
}

static void strike(void)
{
    s_count++;
    lv_label_set_text_fmt(s_count_label, "%lu 次", (unsigned long)s_count);
    float_feedback_create();
    impact_show();
    if (s_audio_task) {
        xTaskNotifyGive(s_audio_task);
    }

    lv_anim_delete(s_head, striker_set_y);
    striker_set_y(s_head, STRIKER_REST_Y);

    lv_anim_t animation;
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_head);
    lv_anim_set_exec_cb(&animation, striker_set_y);
    lv_anim_set_values(&animation, STRIKER_REST_Y, STRIKER_HIT_Y);
    lv_anim_set_duration(&animation, 90);
    lv_anim_set_playback_duration(&animation, 170);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_in);
    lv_anim_start(&animation);
}

static void on_button(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)button;
    (void)user;

    // PRESS avoids the click/double-click recognition delay and feels immediate.
    if (event != BSP_BTN_PRESS || !bsp_lvgl_lock(100)) {
        return;
    }
    strike();
    bsp_lvgl_unlock();
}

static lv_obj_t *impact_line_create(const lv_point_precise_t *points)
{
    lv_obj_t *line = lv_line_create(s_screen);
    lv_line_set_points(line, points, 2);
    lv_obj_set_style_line_width(line, 2, 0);
    lv_obj_set_style_line_color(line, lv_color_hex(COLOR_TEXT), 0);
    lv_obj_set_style_line_rounded(line, true, 0);
    lv_obj_add_flag(line, LV_OBJ_FLAG_HIDDEN);
    return line;
}

static void create_ui(void)
{
    static const lv_point_precise_t impact_a[] = {{132, 153}, {122, 143}};
    static const lv_point_precise_t impact_b[] = {{137, 148}, {134, 135}};
    static const lv_point_precise_t impact_c[] = {{127, 158}, {114, 154}};

    s_screen = lv_obj_create(NULL);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(0x302A31), 0);
    lv_obj_set_style_border_width(s_screen, 0, 0);
    lv_obj_set_style_pad_all(s_screen, 0, 0);

    lv_obj_t *background = lv_image_create(s_screen);
    lv_image_set_src(background, &s_muyu_bg);
    lv_obj_set_pos(background, 0, 0);

    lv_obj_t *title = label_create(s_screen, "今日敲击");
    lv_obj_set_pos(title, 18, 31);

    s_count_label = label_create(s_screen, "0 次");
    lv_obj_set_pos(s_count_label, 18, 58);

    s_handle = line_create(s_screen, s_handle_points, 10, COLOR_HANDLE);
    s_handle_highlight = line_create(s_screen, s_highlight_points, 3,
                                     COLOR_HANDLE_LIGHT);

    s_head = lv_obj_create(s_screen);
    lv_obj_remove_flag(s_head, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(s_head, 28, 28);
    lv_obj_set_style_radius(s_head, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_head, lv_color_hex(COLOR_HEAD), 0);
    lv_obj_set_style_bg_grad_color(s_head, lv_color_white(), 0);
    lv_obj_set_style_bg_grad_dir(s_head, LV_GRAD_DIR_HOR, 0);
    lv_obj_set_style_border_color(s_head, lv_color_hex(COLOR_HEAD_BORDER), 0);
    lv_obj_set_style_border_width(s_head, 1, 0);
    lv_obj_set_style_shadow_width(s_head, 5, 0);
    lv_obj_set_style_shadow_opa(s_head, LV_OPA_30, 0);
    lv_obj_set_style_pad_all(s_head, 0, 0);

    s_impact[0] = impact_line_create(impact_a);
    s_impact[1] = impact_line_create(impact_b);
    s_impact[2] = impact_line_create(impact_c);
    s_impact_timer = lv_timer_create(impact_hide, 180, NULL);
    lv_timer_pause(s_impact_timer);

    striker_set_y(s_head, STRIKER_REST_Y);
    lv_screen_load(s_screen);
}

void app_main(void)
{
    ESP_LOGI(TAG, "FoloToy AI Passport wooden-fish app starting");

    bsp_i2c_init();
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "LVGL lock timeout");
        return;
    }
    create_ui();
    bsp_lvgl_unlock();

    bool audio_ok = audio_start();

    if (bsp_button_init(on_button, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "button init failed");
        return;
    }

    ESP_LOGI(TAG, "ready: press any hardware button to strike (audio=%d)",
             audio_ok);
}
