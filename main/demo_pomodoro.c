#include "demo.h"

#include <stdio.h>

#include "bsp_audio.h"
#include "bsp_battery.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "pomodoro_model.h"
#include "pomodoro_store.h"

#define COLOR_BG       0x050608
#define COLOR_WHITE    0xF4F1E8
#define COLOR_DIM      0x777C85
#define COLOR_TRACK    0x25282E
#define COLOR_RED      0xF02B24
#define COLOR_RED_DARK 0x9D1718
#define COLOR_GREEN    0x36D765
#define COLOR_BLUE     0x42B9F5
#define COLOR_YELLOW   0xFFD65A

#define CAT_HOME_X 178
#define CAT_HOME_Y 255

#define TONE_SAMPLE_RATE 16000
#define TONE_CHUNK       128

typedef enum {
    TONE_START = 1,
    TONE_PAUSE,
    TONE_COMPLETE,
    TONE_LEVEL_UP,
} tone_id_t;

static const char *TAG = "pomo";
static const char *CAT_NAMES[] = {
    "TINY KITTEN", "CURIOUS KITTEN", "PLAYFUL CAT", "FOCUS CAT", "MASTER CAT"
};

static pomodoro_model_t s_model;
static bool s_prepared;
static bool s_audio_ok;
static bool s_battery_ok;
static QueueHandle_t s_audio_queue;
static volatile bool s_audio_cancel;

static lv_obj_t *s_scr;
static lv_obj_t *s_round_label;
static lv_obj_t *s_battery_label;
static lv_obj_t *s_battery_icon;
static lv_obj_t *s_brand;
static lv_obj_t *s_title;
static lv_obj_t *s_ring;
static lv_obj_t *s_tick_layer;
static lv_obj_t *s_tomato;
static lv_obj_t *s_time_label;
static lv_obj_t *s_state_label;
static lv_obj_t *s_growth_label;
static lv_obj_t *s_growth_bar;
static lv_obj_t *s_action_icon;
static lv_obj_t *s_cat;
static lv_obj_t *s_heart;
static lv_obj_t *s_pet_stats;
static lv_timer_t *s_timer;
static bool s_pet_view;
static int s_battery_soc = -1;
static uint32_t s_last_sec = UINT32_MAX;
static uint32_t s_save_bucket = UINT32_MAX;
static uint8_t s_drawn_stage = UINT8_MAX;

static uint64_t now_ms(void) {
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static lv_obj_t *container(lv_obj_t *parent, int x, int y, int w, int h) {
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent, const lv_font_t *font,
                       uint32_t color) {
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_set_style_text_font(obj, font, 0);
    lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    return obj;
}

static void draw_pixel_rect(lv_layer_t *layer, const lv_area_t *base,
                            int x, int y, int w, int h,
                            uint32_t color, int radius) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.base.layer = layer;
    dsc.bg_color = lv_color_hex(color);
    dsc.bg_opa = LV_OPA_COVER;
    dsc.radius = radius;
    lv_area_t area = {
        .x1 = base->x1 + x,
        .y1 = base->y1 + y,
        .x2 = base->x1 + x + w - 1,
        .y2 = base->y1 + y + h - 1,
    };
    lv_draw_rect(layer, &dsc, &area);
}

static lv_obj_t *draw_layer_create(lv_obj_t *parent, int x, int y, int w, int h,
                                   lv_event_cb_t draw_cb) {
    lv_obj_t *obj = container(parent, x, y, w, h);
    lv_obj_add_event_cb(obj, draw_cb, LV_EVENT_DRAW_MAIN, NULL);
    return obj;
}

static lv_obj_t *brand_create(lv_obj_t *parent) {
    enum { BRAND_WIDTH = 104, BRAND_HEIGHT = 21, BRAND_STRIDE = 13 };
    static const uint8_t BRAND_BITS[] = {
        0x00, 0x00, 0x00, 0x46, 0x20, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x40, 0x30,
        0x00, 0x00, 0x00, 0xE6, 0x60, 0x00, 0x0F, 0xC0, 0x03, 0x0C, 0x00, 0xE0, 0x20,
        0xFF, 0xFC, 0x01, 0xC6, 0x50, 0x1F, 0xFF, 0xC0, 0x03, 0x0C, 0x00, 0xC0, 0x20,
        0xFF, 0xFC, 0x01, 0x82, 0x3C, 0x1E, 0x32, 0x00, 0xFF, 0xFF, 0xF1, 0x80, 0x20,
        0xC0, 0x0C, 0x03, 0x87, 0xF8, 0x03, 0x33, 0x00, 0xFF, 0xFF, 0xF1, 0xF8, 0x20,
        0xC0, 0x0C, 0x07, 0xFF, 0x00, 0x03, 0x36, 0x00, 0x03, 0x0C, 0x03, 0x03, 0xFF,
        0xC0, 0x0C, 0x0D, 0xA1, 0x80, 0x3F, 0xFF, 0xF0, 0x13, 0x0C, 0x03, 0x02, 0x21,
        0xC0, 0x0C, 0x01, 0x80, 0xCC, 0x00, 0xBC, 0x00, 0x1C, 0x00, 0x06, 0x02, 0x21,
        0xC0, 0x0C, 0x01, 0x8C, 0x7C, 0x01, 0xB6, 0x00, 0x18, 0x1F, 0xC5, 0xFA, 0x21,
        0xC0, 0x0C, 0x01, 0x8C, 0x3C, 0x03, 0x33, 0x80, 0xFF, 0x9F, 0xC0, 0x42, 0x21,
        0xC0, 0x0C, 0x00, 0x10, 0x00, 0x1E, 0x31, 0xF0, 0xFF, 0x98, 0xC0, 0x42, 0x21,
        0xC0, 0x0C, 0x0F, 0xFF, 0xFE, 0x3C, 0x00, 0x60, 0x19, 0x98, 0xC0, 0x42, 0x21,
        0xC0, 0x0C, 0x00, 0x36, 0x00, 0x37, 0xFF, 0x80, 0x19, 0x98, 0xC3, 0xFB, 0xFF,
        0xC0, 0x0C, 0x00, 0x73, 0x30, 0x06, 0x21, 0x80, 0x19, 0x98, 0xC0, 0x42, 0x21,
        0xC0, 0x0C, 0x01, 0xE1, 0x68, 0x06, 0x21, 0x80, 0x31, 0x98, 0xC0, 0x42, 0x20,
        0xFF, 0xFC, 0x07, 0x61, 0xC0, 0x07, 0xFF, 0x80, 0x31, 0x98, 0xC0, 0x58, 0x20,
        0xFF, 0xFC, 0x0E, 0x64, 0xF0, 0x06, 0x21, 0x80, 0x61, 0x9F, 0xC0, 0x78, 0x20,
        0xC0, 0x0C, 0x08, 0x6C, 0x3E, 0x06, 0x21, 0x80, 0x61, 0x9F, 0xC0, 0x70, 0x20,
        0xC0, 0x0C, 0x00, 0x78, 0x0C, 0x07, 0xFF, 0x80, 0xCF, 0x18, 0xC1, 0xC0, 0x20,
        0x00, 0x00, 0x00, 0xE0, 0x00, 0x06, 0x01, 0x81, 0x86, 0x18, 0x00, 0x80, 0x20,
        0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    };
    LV_DRAW_BUF_DEFINE_STATIC(brand_buf, BRAND_WIDTH, BRAND_HEIGHT, LV_COLOR_FORMAT_RGB565);
    LV_DRAW_BUF_INIT_STATIC(brand_buf);

    lv_obj_t *canvas = lv_canvas_create(parent);
    lv_canvas_set_draw_buf(canvas, &brand_buf);
    lv_canvas_fill_bg(canvas, lv_color_hex(COLOR_BG), LV_OPA_COVER);
    for (int y = 0; y < BRAND_HEIGHT; y++) {
        for (int x = 0; x < BRAND_WIDTH; x++) {
            uint8_t bits = BRAND_BITS[y * BRAND_STRIDE + x / 8];
            if (bits & (0x80U >> (x % 8))) {
                lv_canvas_set_px(canvas, x, y, lv_color_hex(COLOR_WHITE), LV_OPA_COVER);
            }
        }
    }
    lv_obj_set_pos(canvas, (240 - BRAND_WIDTH) / 2, 30);
    return canvas;
}

static void tomato_draw_cb(lv_event_t *event) {
    static const uint8_t ROWS[][4] = {
        { 43, 35, 40, 8 }, { 30, 43, 66, 8 }, { 20, 51, 86, 8 },
        { 13, 59, 100, 8 }, { 8, 67, 110, 8 }, { 5, 75, 116, 8 },
        { 5, 83, 116, 8 }, { 7, 91, 112, 8 }, { 11, 99, 104, 8 },
        { 17, 107, 92, 8 }, { 25, 115, 76, 8 }, { 36, 123, 54, 8 },
    };
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t base;
    lv_obj_get_coords(obj, &base);
    for (size_t i = 0; i < sizeof(ROWS) / sizeof(ROWS[0]); i++) {
        draw_pixel_rect(layer, &base, ROWS[i][0], ROWS[i][1],
                        ROWS[i][2], ROWS[i][3], COLOR_RED, 0);
    }

    /* Reference-like lower-right volume: broad crimson shadow with a dark base. */
    draw_pixel_rect(layer, &base, 91, 67, 23, 16, 0xDE211E, 0);
    draw_pixel_rect(layer, &base, 83, 83, 34, 16, 0xDE211E, 0);
    draw_pixel_rect(layer, &base, 73, 99, 41, 16, 0xD31D1C, 0);
    draw_pixel_rect(layer, &base, 57, 115, 44, 8, COLOR_RED_DARK, 0);
    draw_pixel_rect(layer, &base, 36, 123, 54, 8, COLOR_RED_DARK, 0);

    /* Chunky specular highlight from the photographed pixel-art reference. */
    draw_pixel_rect(layer, &base, 26, 55, 10, 17, COLOR_WHITE, 1);
    draw_pixel_rect(layer, &base, 35, 48, 13, 10, COLOR_WHITE, 1);
    draw_pixel_rect(layer, &base, 20, 70, 7, 9, 0xFFC0B4, 0);

    /* Three-tone leafy crown and upright stem. */
    draw_pixel_rect(layer, &base, 55, 12, 10, 28, COLOR_GREEN, 0);
    draw_pixel_rect(layer, &base, 37, 27, 30, 9, COLOR_GREEN, 0);
    draw_pixel_rect(layer, &base, 62, 24, 34, 9, 0x25B74D, 0);
    draw_pixel_rect(layer, &base, 78, 17, 9, 20, 0x25B74D, 0);
    draw_pixel_rect(layer, &base, 47, 21, 9, 18, 0x48E46F, 0);
    draw_pixel_rect(layer, &base, 30, 31, 19, 8, 0x25B74D, 0);
}

static lv_obj_t *tomato_create(lv_obj_t *parent) {
    return draw_layer_create(parent, 58, 87, 124, 137, tomato_draw_cb);
}

static void heart_draw_cb(lv_event_t *event) {
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t base;
    lv_obj_get_coords(obj, &base);
    draw_pixel_rect(layer, &base, 2, 2, 6, 6, COLOR_GREEN, 1);
    draw_pixel_rect(layer, &base, 10, 2, 6, 6, COLOR_GREEN, 1);
    draw_pixel_rect(layer, &base, 0, 6, 18, 5, COLOR_GREEN, 1);
    draw_pixel_rect(layer, &base, 4, 11, 10, 3, COLOR_GREEN, 0);
    draw_pixel_rect(layer, &base, 7, 14, 4, 3, COLOR_GREEN, 0);
}

static lv_obj_t *heart_create(lv_obj_t *parent) {
    return draw_layer_create(parent, 183, 237, 18, 17, heart_draw_cb);
}

static void cat_draw_cb(lv_event_t *event) {
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t base;
    lv_obj_get_coords(obj, &base);
    uint8_t stage = pomodoro_model_cat_stage(&s_model);
    uint32_t fur = stage >= 3 ? 0xFFF4DE : COLOR_WHITE;

    /* Red ears sit behind the square white face. */
    draw_pixel_rect(layer, &base, 5, 2, 13, 17, COLOR_RED, 0);
    draw_pixel_rect(layer, &base, 40, 2, 13, 17, COLOR_RED, 0);
    draw_pixel_rect(layer, &base, 8, 5, 6, 8, 0xFF9C93, 0);
    draw_pixel_rect(layer, &base, 44, 5, 6, 8, 0xFF9C93, 0);

    /* Stepped, almost-square head silhouette from the reference mascot. */
    draw_pixel_rect(layer, &base, 10, 6, 38, 4, fur, 0);
    draw_pixel_rect(layer, &base, 7, 10, 44, 5, fur, 0);
    draw_pixel_rect(layer, &base, 4, 15, 50, 24, fur, 1);
    draw_pixel_rect(layer, &base, 8, 39, 42, 6, fur, 0);
    draw_pixel_rect(layer, &base, 6, 16, 8, 11, COLOR_RED, 0);
    draw_pixel_rect(layer, &base, 43, 14, 8, 8, 0xE8D7C7, 0);

    /* Bold facial pixels: black eyes, red nose and tiny smiling mouth. */
    draw_pixel_rect(layer, &base, 14, 24, 6, 7, 0x202329, 0);
    draw_pixel_rect(layer, &base, 37, 24, 6, 7, 0x202329, 0);
    draw_pixel_rect(layer, &base, 16, 24, 2, 2, COLOR_WHITE, 0);
    draw_pixel_rect(layer, &base, 39, 24, 2, 2, COLOR_WHITE, 0);
    draw_pixel_rect(layer, &base, 26, 30, 6, 5, COLOR_RED, 0);
    draw_pixel_rect(layer, &base, 22, 35, 5, 3, 0x5D3330, 0);
    draw_pixel_rect(layer, &base, 31, 35, 5, 3, 0x5D3330, 0);

    /* Red suit, white chest and squared paws. */
    draw_pixel_rect(layer, &base, 12, 44, 34, 15, COLOR_RED, 1);
    draw_pixel_rect(layer, &base, 19, 44, 20, 15, fur, 0);
    draw_pixel_rect(layer, &base, 4, 47, 11, 10, fur, 1);
    draw_pixel_rect(layer, &base, 43, 47, 11, 10, fur, 1);
    draw_pixel_rect(layer, &base, 9, 57, 16, 7, fur, 1);
    draw_pixel_rect(layer, &base, 33, 57, 16, 7, fur, 1);
    draw_pixel_rect(layer, &base, 16, 59, 26, 5, COLOR_RED_DARK, 0);

    if (stage >= 1) {
        draw_pixel_rect(layer, &base, 8, 41, 42, 4, 0xFF5A4F, 0);
    }
    if (stage >= 2) {
        draw_pixel_rect(layer, &base, 45, 35, 10, 12, 0xE69B5B, 2);
    }
    if (stage >= 3) {
        draw_pixel_rect(layer, &base, 11, 19, 13, 3, COLOR_BLUE, 0);
        draw_pixel_rect(layer, &base, 29, 19, 13, 3, COLOR_BLUE, 0);
        draw_pixel_rect(layer, &base, 24, 20, 5, 2, COLOR_BLUE, 0);
    }
    if (stage >= 4) {
        draw_pixel_rect(layer, &base, 13, 0, 27, 5, COLOR_YELLOW, 0);
        draw_pixel_rect(layer, &base, 14, -4, 5, 7, COLOR_YELLOW, 0);
        draw_pixel_rect(layer, &base, 24, -6, 5, 9, COLOR_YELLOW, 0);
        draw_pixel_rect(layer, &base, 34, -4, 5, 7, COLOR_YELLOW, 0);
    }
}

static lv_obj_t *cat_create(lv_obj_t *parent) {
    return draw_layer_create(parent, CAT_HOME_X, CAT_HOME_Y, 58, 65, cat_draw_cb);
}

static void draw_cat_if_needed(void) {
    uint8_t stage = pomodoro_model_cat_stage(&s_model);
    if (s_cat && s_drawn_stage == stage) return;
    if (!s_cat) s_cat = cat_create(s_scr);
    s_drawn_stage = stage;
    lv_obj_invalidate(s_cat);
    if (s_pet_view) lv_obj_set_pos(s_cat, 91, 87);
}

static void ticks_draw_cb(lv_event_t *event) {
    static const int16_t X[] = { 118, 164, 196, 208, 196, 164, 118, 72, 40, 27, 40, 72 };
    static const int16_t Y[] = { 57, 70, 103, 150, 197, 230, 242, 230, 197, 150, 103, 70 };
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t base;
    lv_obj_get_coords(obj, &base);
    for (size_t i = 0; i < sizeof(X) / sizeof(X[0]); i++) {
        bool major = (i % 3) == 0;
        draw_pixel_rect(layer, &base, X[i], Y[i], major ? 4 : 3,
                        major ? 10 : 7, major ? COLOR_WHITE : COLOR_DIM, 0);
    }
    draw_pixel_rect(layer, &base, 43, 87, 6, 19, 0xF26A2E, 2);
    draw_pixel_rect(layer, &base, 53, 76, 6, 15, 0xFFE4CF, 2);
}

static lv_obj_t *ticks_create(lv_obj_t *parent) {
    return draw_layer_create(parent, 0, 0, 240, 320, ticks_draw_cb);
}

static void battery_draw_cb(lv_event_t *event) {
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t base;
    lv_obj_get_coords(obj, &base);
    draw_pixel_rect(layer, &base, 0, 0, 17, 10, COLOR_DIM, 2);
    draw_pixel_rect(layer, &base, 17, 3, 3, 4, COLOR_DIM, 0);
    draw_pixel_rect(layer, &base, 2, 2, 13, 6, COLOR_BG, 0);
    int fill = s_battery_soc < 0 ? 0 : (s_battery_soc * 11 + 99) / 100;
    if (fill > 0) draw_pixel_rect(layer, &base, 3, 3, fill, 4, COLOR_GREEN, 0);
}

static lv_obj_t *battery_icon_create(lv_obj_t *parent) {
    return draw_layer_create(parent, 217, 8, 20, 10, battery_draw_cb);
}

static void action_draw_cb(lv_event_t *event) {
    lv_obj_t *obj = lv_event_get_current_target(event);
    lv_layer_t *layer = lv_event_get_layer(event);
    lv_area_t base;
    lv_obj_get_coords(obj, &base);
    draw_pixel_rect(layer, &base, 0, 1, 3, 14, COLOR_RED, 0);
    draw_pixel_rect(layer, &base, 3, 3, 3, 10, COLOR_RED, 0);
    draw_pixel_rect(layer, &base, 6, 5, 3, 6, COLOR_RED, 0);
    draw_pixel_rect(layer, &base, 9, 7, 2, 2, COLOR_RED, 0);
}

static lv_obj_t *action_icon_create(lv_obj_t *parent) {
    return draw_layer_create(parent, 27, 287, 11, 16, action_draw_cb);
}

static void audio_write_note(int frequency, int duration_ms) {
    int16_t samples[TONE_CHUNK];
    int total = TONE_SAMPLE_RATE * duration_ms / 1000;
    int period = frequency > 0 ? TONE_SAMPLE_RATE / frequency : 1;
    int phase = 0;
    while (total > 0 && !s_audio_cancel) {
        int count = total < TONE_CHUNK ? total : TONE_CHUNK;
        for (int i = 0; i < count; i++) {
            samples[i] = frequency == 0 ? 0 : (phase < period / 2 ? 4200 : -4200);
            if (++phase >= period) phase = 0;
        }
        bsp_audio_write(samples, (size_t)count * sizeof(samples[0]));
        total -= count;
    }
}

static void audio_task(void *arg) {
    (void)arg;
    uint8_t tone;
    bsp_audio_set_format(TONE_SAMPLE_RATE, 16, 1);
    bsp_audio_set_volume(55);
    while (true) {
        if (xQueueReceive(s_audio_queue, &tone, portMAX_DELAY) != pdTRUE) continue;
        if (tone == TONE_START) {
            audio_write_note(880, 90);
            audio_write_note(1175, 110);
        } else if (tone == TONE_PAUSE) {
            audio_write_note(660, 100);
        } else if (tone == TONE_COMPLETE || tone == TONE_LEVEL_UP) {
            audio_write_note(784, 120);
            audio_write_note(988, 120);
            audio_write_note(1319, tone == TONE_LEVEL_UP ? 420 : 260);
        }
    }
}

static void play_tone(tone_id_t tone) {
    if (!s_audio_ok || s_model.muted || !s_audio_queue) return;
    s_audio_cancel = false;
    uint8_t value = tone;
    xQueueOverwrite(s_audio_queue, &value);
}

void demo_pomodoro_prepare(bool audio_ok, bool battery_ok) {
    if (s_prepared) return;
    s_prepared = true;
    s_audio_ok = audio_ok;
    s_battery_ok = battery_ok;
    pomodoro_model_defaults(&s_model);
    pomodoro_store_init(&s_model);

    if (s_audio_ok) {
        s_audio_queue = xQueueCreate(1, sizeof(uint8_t));
        if (!s_audio_queue ||
            xTaskCreate(audio_task, "pomo_audio", 3072, NULL, 4, NULL) != pdPASS) {
            ESP_LOGW(TAG, "Audio feedback disabled: worker creation failed");
            s_audio_ok = false;
        }
    }
}

static void set_hidden(lv_obj_t *obj, bool hidden) {
    if (!obj) return;
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

static uint32_t displayed_seconds(void) {
    if (s_model.state == POMODORO_BREAK_RUNNING ||
        s_model.state == POMODORO_BREAK_PAUSED ||
        s_model.state == POMODORO_BREAK_PROMPT) {
        return s_model.state == POMODORO_BREAK_PROMPT
                   ? s_model.pending_break_min * 60U
                   : s_model.break_remaining_sec;
    }
    return s_model.state == POMODORO_IDLE
               ? pomodoro_model_focus_min(&s_model) * 60U
               : s_model.remaining_sec;
}

static void refresh_pet_card(void) {
    uint8_t stage = pomodoro_model_cat_stage(&s_model);
    uint32_t current, target;
    pomodoro_model_growth(&s_model, &current, &target);
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_20, 0);
    lv_label_set_text(s_title, "MY FOCUS CAT");
    lv_label_set_text_fmt(s_pet_stats,
        "%s\n\nFOCUS SESSIONS  %lu\nFOCUS MINUTES   %lu\nBOND            %lu/%lu\n\nOK: BACK",
        CAT_NAMES[stage], (unsigned long)s_model.completed_sessions,
        (unsigned long)s_model.completed_focus_min,
        (unsigned long)current, (unsigned long)target);
}

static void refresh_ui(void) {
    if (!s_scr) return;
    draw_cat_if_needed();
    if (s_pet_view) {
        refresh_pet_card();
        return;
    }

    uint32_t seconds = displayed_seconds();
    uint32_t total = pomodoro_model_focus_min(&s_model) * 60U;
    bool is_break = s_model.state == POMODORO_BREAK_PROMPT ||
                    s_model.state == POMODORO_BREAK_RUNNING ||
                    s_model.state == POMODORO_BREAK_PAUSED;
    if (is_break) total = s_model.pending_break_min * 60U;
    int arc_value = total ? (int)((uint64_t)seconds * 1000 / total) : 0;

    lv_label_set_text_fmt(s_round_label, "FOCUS %u/4", s_model.pomodoro_round + 1);
    if (s_battery_soc >= 0) {
        lv_label_set_text_fmt(s_battery_label, "%d%%%s", s_battery_soc,
                              s_model.muted ? " M" : "");
    } else {
        lv_label_set_text_fmt(s_battery_label, "--%%%s", s_model.muted ? " M" : "");
    }
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_RIGHT, -27, 5);
    if (s_battery_icon) lv_obj_invalidate(s_battery_icon);
    lv_arc_set_value(s_ring, arc_value);
    lv_obj_set_style_arc_color(s_ring,
        lv_color_hex(is_break ? COLOR_GREEN : COLOR_RED), LV_PART_INDICATOR);
    lv_label_set_text_fmt(s_time_label, "%02lu:%02lu",
                          (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));

    switch (s_model.state) {
        case POMODORO_IDLE:
            lv_label_set_text(s_state_label, "PLAY");
            break;
        case POMODORO_FOCUS_RUNNING:
            lv_label_set_text(s_state_label, "PAUSE");
            break;
        case POMODORO_FOCUS_PAUSED:
            lv_label_set_text(s_state_label, "RESUME");
            break;
        case POMODORO_ABANDON_CONFIRM:
            lv_label_set_text(s_state_label, "ABANDON?");
            break;
        case POMODORO_REWARD:
            lv_label_set_text(s_state_label, "GROW!");
            break;
        case POMODORO_BREAK_PROMPT:
            lv_label_set_text(s_state_label, "REST");
            break;
        case POMODORO_BREAK_RUNNING:
            lv_label_set_text(s_state_label, "PAUSE");
            break;
        case POMODORO_BREAK_PAUSED:
            lv_label_set_text(s_state_label, "RESUME");
            break;
    }

    lv_bar_set_range(s_growth_bar, 0, 1000);
    lv_bar_set_value(s_growth_bar, arc_value, LV_ANIM_ON);
}

static void set_pet_view(bool enabled) {
    s_pet_view = enabled;
    set_hidden(s_brand, enabled);
    set_hidden(s_title, !enabled);
    set_hidden(s_ring, enabled);
    set_hidden(s_tick_layer, enabled);
    set_hidden(s_tomato, enabled);
    set_hidden(s_time_label, enabled);
    set_hidden(s_state_label, enabled);
    set_hidden(s_growth_label, enabled);
    set_hidden(s_growth_bar, enabled);
    set_hidden(s_action_icon, enabled);
    set_hidden(s_heart, enabled);
    set_hidden(s_pet_stats, !enabled);
    if (s_cat) lv_obj_set_pos(s_cat, enabled ? 91 : CAT_HOME_X,
                              enabled ? 87 : CAT_HOME_Y);
    refresh_ui();
}

static void persist(void) {
    pomodoro_store_request_save(&s_model);
}

static void timer_cb(lv_timer_t *timer) {
    (void)timer;
    uint64_t time_ms = now_ms();
    uint8_t stage_before = pomodoro_model_cat_stage(&s_model);
    pomodoro_event_t event = pomodoro_model_tick(&s_model, time_ms);
    if (event == POMODORO_EVENT_FOCUS_COMPLETE) {
        persist();
        uint8_t stage_after = pomodoro_model_cat_stage(&s_model);
        play_tone(stage_after > stage_before ? TONE_LEVEL_UP : TONE_COMPLETE);
    } else if (event == POMODORO_EVENT_REWARD_FINISHED ||
               event == POMODORO_EVENT_BREAK_COMPLETE ||
               event == POMODORO_EVENT_CONFIRM_TIMEOUT) {
        persist();
    }

    uint32_t seconds = displayed_seconds();
    if (seconds != s_last_sec || event != POMODORO_EVENT_NONE) {
        s_last_sec = seconds;
        refresh_ui();
    }

    if (s_model.state == POMODORO_FOCUS_RUNNING ||
        s_model.state == POMODORO_BREAK_RUNNING) {
        uint32_t bucket = seconds / 60;
        if (bucket != s_save_bucket) {
            s_save_bucket = bucket;
            persist();
        }
    }

    if (s_model.state == POMODORO_REWARD) {
        uint32_t phase = (uint32_t)(time_ms % 800);
        int jump = phase < 400 ? (int)(phase / 50) : (int)((800 - phase) / 50);
        if (s_cat) lv_obj_set_y(s_cat, CAT_HOME_Y - jump);
        set_hidden(s_heart, (phase / 160) % 2);
    } else if (!s_pet_view) {
        if (s_cat) lv_obj_set_y(s_cat, CAT_HOME_Y);
        set_hidden(s_heart, false);
    }

    static uint32_t battery_ticks;
    if (++battery_ticks >= 150) {
        battery_ticks = 0;
        if (s_battery_ok) s_battery_soc = bsp_battery_soc();
        refresh_ui();
    }
}

void demo_pomodoro_enter(void) {
    lv_mem_monitor_t memory_before;
    lv_mem_monitor(&memory_before);
    if (!s_prepared) demo_pomodoro_prepare(false, false);
    s_pet_view = false;
    s_last_sec = UINT32_MAX;
    s_save_bucket = UINT32_MAX;
    s_drawn_stage = UINT8_MAX;
    if (s_battery_ok) s_battery_soc = bsp_battery_soc();

    s_scr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(COLOR_BG), 0);
    lv_obj_set_style_bg_opa(s_scr, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);
    lv_obj_clear_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);

    s_round_label = label(s_scr, &lv_font_montserrat_14, COLOR_DIM);
    lv_obj_set_pos(s_round_label, 9, 5);
    s_battery_label = label(s_scr, &lv_font_montserrat_14, COLOR_DIM);
    lv_obj_align(s_battery_label, LV_ALIGN_TOP_RIGHT, -27, 5);
    s_battery_icon = battery_icon_create(s_scr);
    s_brand = brand_create(s_scr);
    s_title = label(s_scr, &lv_font_montserrat_20, COLOR_WHITE);
    lv_obj_set_width(s_title, 240);
    lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_title, 0, 31);
    set_hidden(s_title, true);

    s_tick_layer = ticks_create(s_scr);
    s_ring = lv_arc_create(s_scr);
    lv_obj_set_size(s_ring, 182, 182);
    lv_obj_set_pos(s_ring, 29, 64);
    lv_arc_set_range(s_ring, 0, 1000);
    lv_arc_set_rotation(s_ring, 270);
    lv_arc_set_bg_angles(s_ring, 0, 360);
    lv_obj_remove_style(s_ring, NULL, LV_PART_KNOB);
    lv_obj_set_style_arc_width(s_ring, 9, LV_PART_MAIN);
    lv_obj_set_style_arc_color(s_ring, lv_color_hex(COLOR_TRACK), LV_PART_MAIN);
    lv_obj_set_style_arc_width(s_ring, 9, LV_PART_INDICATOR);
    lv_obj_clear_flag(s_ring, LV_OBJ_FLAG_CLICKABLE);

    s_tomato = tomato_create(s_scr);
    s_time_label = label(s_scr, &lv_font_montserrat_20, COLOR_WHITE);
    lv_obj_set_width(s_time_label, 240);
    lv_obj_set_style_text_align(s_time_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_time_label, 0, 232);
    s_action_icon = action_icon_create(s_scr);
    s_state_label = label(s_scr, &lv_font_montserrat_14, COLOR_RED);
    lv_obj_set_width(s_state_label, 70);
    lv_obj_set_style_text_align(s_state_label, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_pos(s_state_label, 42, 286);
    s_growth_label = NULL;
    s_growth_bar = lv_bar_create(s_scr);
    lv_obj_set_pos(s_growth_bar, 88, 293);
    lv_obj_set_size(s_growth_bar, 76, 10);
    lv_obj_set_style_bg_color(s_growth_bar, lv_color_hex(COLOR_TRACK), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(s_growth_bar, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_growth_bar, lv_color_hex(COLOR_GREEN), LV_PART_INDICATOR);

    s_heart = heart_create(s_scr);
    s_pet_stats = label(s_scr, &lv_font_montserrat_14, COLOR_WHITE);
    lv_obj_set_width(s_pet_stats, 220);
    lv_obj_set_style_text_align(s_pet_stats, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_pos(s_pet_stats, 10, 165);
    set_hidden(s_pet_stats, true);

    draw_cat_if_needed();
    refresh_ui();
    s_timer = lv_timer_create(timer_cb, 200, NULL);
    lv_mem_monitor_t memory_after;
    lv_mem_monitor(&memory_after);
    ESP_LOGI(TAG, "LVGL memory: %u -> %u bytes free, %u%% used, %u%% fragmented",
             (unsigned)memory_before.free_size, (unsigned)memory_after.free_size,
             memory_after.used_pct, memory_after.frag_pct);
    lv_screen_load(s_scr);
}

void demo_pomodoro_exit(void) {
    if (s_model.state == POMODORO_FOCUS_RUNNING ||
        s_model.state == POMODORO_BREAK_RUNNING) {
        pomodoro_model_pause(&s_model, now_ms());
    }
    persist();
    s_audio_cancel = true;
    if (s_audio_queue) xQueueReset(s_audio_queue);
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_scr) lv_obj_delete(s_scr);
    s_scr = s_round_label = s_battery_label = s_battery_icon = s_brand = NULL;
    s_title = s_ring = s_tick_layer = NULL;
    s_tomato = s_time_label = s_state_label = s_growth_label = NULL;
    s_growth_bar = s_action_icon = s_cat = s_heart = s_pet_stats = NULL;
}

void demo_pomodoro_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (s_pet_view) {
        if (btn == BSP_BTN_OK && (ev == BSP_BTN_CLICK || ev == BSP_BTN_DOUBLE)) {
            set_pet_view(false);
        }
        return;
    }

    if (ev == BSP_BTN_DOUBLE) {
        if (s_model.state == POMODORO_IDLE && btn == BSP_BTN_OK) {
            set_pet_view(true);
        } else if (s_model.state == POMODORO_IDLE && btn == BSP_BTN_DOWN) {
            s_model.muted = !s_model.muted;
            persist();
            refresh_ui();
        }
        return;
    }
    if (ev != BSP_BTN_CLICK) return;

    bool changed = false;
    uint64_t time_ms = now_ms();
    switch (s_model.state) {
        case POMODORO_IDLE:
            if (btn == BSP_BTN_OK) {
                changed = pomodoro_model_start_focus(&s_model, time_ms);
                if (changed) play_tone(TONE_START);
            } else if (btn == BSP_BTN_UP) {
                changed = pomodoro_model_select_duration(&s_model, -1);
            } else if (btn == BSP_BTN_DOWN) {
                changed = pomodoro_model_select_duration(&s_model, 1);
            }
            break;
        case POMODORO_FOCUS_RUNNING:
        case POMODORO_BREAK_RUNNING:
            if (btn == BSP_BTN_OK) {
                changed = pomodoro_model_pause(&s_model, time_ms);
                if (changed) play_tone(TONE_PAUSE);
            }
            break;
        case POMODORO_FOCUS_PAUSED:
            if (btn == BSP_BTN_OK) changed = pomodoro_model_resume(&s_model, time_ms);
            else if (btn == BSP_BTN_UP) {
                changed = pomodoro_model_request_abandon(&s_model, time_ms);
            }
            break;
        case POMODORO_ABANDON_CONFIRM:
            if (btn == BSP_BTN_UP) changed = pomodoro_model_confirm_abandon(&s_model);
            else if (btn == BSP_BTN_DOWN || btn == BSP_BTN_OK) {
                changed = pomodoro_model_cancel_abandon(&s_model);
            }
            break;
        case POMODORO_BREAK_PROMPT:
            if (btn == BSP_BTN_OK) changed = pomodoro_model_start_break(&s_model, time_ms);
            else if (btn == BSP_BTN_UP) changed = pomodoro_model_skip_break(&s_model);
            break;
        case POMODORO_BREAK_PAUSED:
            if (btn == BSP_BTN_OK) changed = pomodoro_model_resume(&s_model, time_ms);
            break;
        case POMODORO_REWARD:
            break;
    }

    if (changed) {
        s_last_sec = UINT32_MAX;
        persist();
        refresh_ui();
    }
}
