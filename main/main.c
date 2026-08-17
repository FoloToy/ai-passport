#include <stdbool.h>
#include <stdint.h>

#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "lvgl.h"

#define COLOR_SKY        0xBDEBFF
#define COLOR_GRASS      0x8BD35A
#define COLOR_GRASS_DARK 0x55A83B
#define COLOR_INK        0x27313D
#define COLOR_WHITE      0xFFFDF7
#define COLOR_PINK       0xFF9FBA
#define COLOR_ORANGE     0xFF8A2A
#define COLOR_GREEN      0x4BAE4F
#define COLOR_PANEL      0xFFF8D9

#define RABBIT_BASE_Y  54
#define HUNGER_START   60
#define HUNGER_STEP    5
#define HUNGER_TICK_MS 15000
#define FEED_AMOUNT    25

static const char *TAG = "white_rabbit";

static lv_obj_t *s_rabbit;
static lv_obj_t *s_carrot;
static lv_obj_t *s_hunger_bar;
static lv_obj_t *s_hunger_label;
static lv_obj_t *s_status_label;
static lv_timer_t *s_feedback_timer;
static int s_hunger = HUNGER_START;

static lv_obj_t *shape(lv_obj_t *parent, int x, int y, int width, int height,
                       uint32_t color, int radius, int border_width)
{
    lv_obj_t *object = lv_obj_create(parent);
    lv_obj_remove_flag(object, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(object, x, y);
    lv_obj_set_size(object, width, height);
    lv_obj_set_style_radius(object, radius, 0);
    lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
    lv_obj_set_style_border_color(object, lv_color_hex(COLOR_INK), 0);
    lv_obj_set_style_border_width(object, border_width, 0);
    lv_obj_set_style_pad_all(object, 0, 0);
    return object;
}

static lv_obj_t *label_create(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    return label;
}

static void rabbit_set_y(void *object, int32_t y)
{
    lv_obj_set_y((lv_obj_t *)object, y);
}

static void rabbit_jump(void)
{
    lv_anim_t animation;

    lv_anim_delete(s_rabbit, rabbit_set_y);
    lv_anim_init(&animation);
    lv_anim_set_var(&animation, s_rabbit);
    lv_anim_set_exec_cb(&animation, rabbit_set_y);
    lv_anim_set_values(&animation, RABBIT_BASE_Y, RABBIT_BASE_Y - 12);
    lv_anim_set_duration(&animation, 130);
    lv_anim_set_playback_duration(&animation, 180);
    lv_anim_set_path_cb(&animation, lv_anim_path_ease_out);
    lv_anim_start(&animation);
}

static const char *hunger_status(void)
{
    if (s_hunger >= 80) {
        return "HAPPY & FULL!";
    }
    if (s_hunger >= 40) {
        return "I'M OK";
    }
    if (s_hunger > 0) {
        return "I'M HUNGRY...";
    }
    return "PLEASE FEED ME!";
}

static void refresh_hunger(bool animate)
{
    lv_bar_set_value(s_hunger_bar, s_hunger,
                     animate ? LV_ANIM_ON : LV_ANIM_OFF);
    lv_label_set_text_fmt(s_hunger_label, "FULLNESS  %d%%", s_hunger);
    lv_label_set_text(s_status_label, hunger_status());
}

static void feedback_done(lv_timer_t *timer)
{
    lv_obj_add_flag(s_carrot, LV_OBJ_FLAG_HIDDEN);
    s_feedback_timer = NULL;
    refresh_hunger(false);
    lv_timer_delete(timer);
}

static void start_feedback_timer(void)
{
    if (s_feedback_timer != NULL) {
        lv_timer_delete(s_feedback_timer);
    }
    s_feedback_timer = lv_timer_create(feedback_done, 850, NULL);
}

static void feed_rabbit(void)
{
    s_hunger += FEED_AMOUNT;
    if (s_hunger > 100) {
        s_hunger = 100;
    }

    lv_obj_remove_flag(s_carrot, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_status_label, "YUM! CARROT!");
    lv_bar_set_value(s_hunger_bar, s_hunger, LV_ANIM_ON);
    lv_label_set_text_fmt(s_hunger_label, "FULLNESS  %d%%", s_hunger);
    rabbit_jump();
    start_feedback_timer();
}

static void hunger_tick(lv_timer_t *timer)
{
    (void)timer;
    if (s_hunger > 0) {
        s_hunger -= HUNGER_STEP;
        if (s_hunger < 0) {
            s_hunger = 0;
        }
        refresh_hunger(true);
    }
}

static void on_button(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (event != BSP_BTN_CLICK || !bsp_lvgl_lock(300)) {
        return;
    }

    if (button == BSP_BTN_OK) {
        feed_rabbit();
    } else if (button == BSP_BTN_UP) {
        lv_label_set_text(s_status_label, "SOFT & HAPPY!");
        rabbit_jump();
        start_feedback_timer();
    }

    bsp_lvgl_unlock();
}

static void create_rabbit(lv_obj_t *screen)
{
    s_rabbit = lv_obj_create(screen);
    lv_obj_remove_flag(s_rabbit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_rabbit, 46, RABBIT_BASE_Y);
    lv_obj_set_size(s_rabbit, 148, 174);
    lv_obj_set_style_bg_opa(s_rabbit, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_rabbit, 0, 0);
    lv_obj_set_style_pad_all(s_rabbit, 0, 0);

    /* Long ears with pink centers. */
    shape(s_rabbit, 29, 2, 34, 78, COLOR_WHITE, 18, 4);
    shape(s_rabbit, 85, 2, 34, 78, COLOR_WHITE, 18, 4);
    shape(s_rabbit, 39, 14, 14, 48, COLOR_PINK, 8, 0);
    shape(s_rabbit, 95, 14, 14, 48, COLOR_PINK, 8, 0);

    /* Body, head, paws, and tail. */
    shape(s_rabbit, 24, 101, 100, 66, COLOR_WHITE, 33, 4);
    shape(s_rabbit, 12, 55, 124, 92, COLOR_WHITE, 46, 4);
    shape(s_rabbit, 13, 133, 42, 27, COLOR_WHITE, 14, 4);
    shape(s_rabbit, 93, 133, 42, 27, COLOR_WHITE, 14, 4);
    shape(s_rabbit, 123, 115, 24, 24, COLOR_WHITE, 12, 4);

    /* Face. */
    shape(s_rabbit, 42, 91, 10, 15, COLOR_INK, 5, 0);
    shape(s_rabbit, 96, 91, 10, 15, COLOR_INK, 5, 0);
    shape(s_rabbit, 68, 109, 12, 9, COLOR_PINK, 5, 0);
    shape(s_rabbit, 28, 112, 20, 9, COLOR_PINK, 5, 0);
    shape(s_rabbit, 100, 112, 20, 9, COLOR_PINK, 5, 0);

    lv_obj_t *mouth = label_create(s_rabbit, "w", &lv_font_montserrat_20,
                                   COLOR_INK);
    lv_obj_set_pos(mouth, 64, 116);
    lv_obj_set_width(mouth, 22);

    /* A tiny carrot appears while feeding. */
    s_carrot = lv_obj_create(s_rabbit);
    lv_obj_remove_flag(s_carrot, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(s_carrot, 112, 83);
    lv_obj_set_size(s_carrot, 28, 54);
    lv_obj_set_style_bg_opa(s_carrot, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_carrot, 0, 0);
    lv_obj_set_style_pad_all(s_carrot, 0, 0);
    shape(s_carrot, 8, 17, 13, 30, COLOR_ORANGE, 6, 0);
    shape(s_carrot, 2, 4, 8, 19, COLOR_GREEN, 4, 0);
    shape(s_carrot, 16, 1, 8, 21, COLOR_GREEN, 4, 0);
    lv_obj_add_flag(s_carrot, LV_OBJ_FLAG_HIDDEN);
}

static void create_ui(void)
{
    lv_obj_t *screen = lv_obj_create(NULL);
    lv_obj_remove_flag(screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(screen, lv_color_hex(COLOR_SKY), 0);
    lv_obj_set_style_border_width(screen, 0, 0);
    lv_obj_set_style_pad_all(screen, 0, 0);

    shape(screen, 0, 214, 240, 106, COLOR_GRASS, 0, 0);
    shape(screen, 0, 214, 240, 5, COLOR_GRASS_DARK, 0, 0);

    lv_obj_t *title = label_create(screen, "BIG WHITE RABBIT",
                                   &lv_font_montserrat_20, COLOR_INK);
    lv_obj_set_pos(title, 8, 10);
    lv_obj_set_width(title, 224);

    create_rabbit(screen);

    lv_obj_t *panel = shape(screen, 15, 224, 210, 58, COLOR_PANEL, 12, 3);
    s_hunger_label = label_create(panel, "", &lv_font_montserrat_14,
                                  COLOR_INK);
    lv_obj_set_pos(s_hunger_label, 8, 5);
    lv_obj_set_width(s_hunger_label, 188);

    s_hunger_bar = lv_bar_create(panel);
    lv_obj_set_pos(s_hunger_bar, 13, 29);
    lv_obj_set_size(s_hunger_bar, 178, 15);
    lv_bar_set_range(s_hunger_bar, 0, 100);
    lv_obj_set_style_bg_color(s_hunger_bar, lv_color_hex(0xD8D4B8), 0);
    lv_obj_set_style_bg_color(s_hunger_bar, lv_color_hex(COLOR_ORANGE),
                              LV_PART_INDICATOR);

    s_status_label = label_create(screen, "", &lv_font_montserrat_14,
                                  COLOR_INK);
    lv_obj_set_pos(s_status_label, 15, 287);
    lv_obj_set_width(s_status_label, 210);

    lv_obj_t *hint = label_create(screen, "OK: FEED   UP: PET",
                                  &lv_font_montserrat_14, COLOR_INK);
    lv_obj_set_pos(hint, 15, 304);
    lv_obj_set_width(hint, 210);

    refresh_hunger(false);
    lv_timer_create(hunger_tick, HUNGER_TICK_MS, NULL);
    lv_screen_load(screen);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Starting the white rabbit pet");
    bsp_i2c_init();

    if (bsp_display_init() != ESP_OK || bsp_lvgl_init() == NULL) {
        ESP_LOGE(TAG, "Display initialization failed (MOSI=%d SCLK=%d CS=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS);
        return;
    }
    bsp_display_backlight(100);

    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "Could not lock LVGL");
        return;
    }
    create_ui();
    bsp_lvgl_unlock();

    if (bsp_button_init(on_button, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "Button initialization failed");
        if (bsp_lvgl_lock(300)) {
            lv_label_set_text(s_status_label, "BUTTON ERROR");
            bsp_lvgl_unlock();
        }
    }
}
