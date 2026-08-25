// main/demo_low_power.c —— light/deep sleep + RTC timer 唤醒验证。
// 不使用按键唤醒：仓库尚无板级唤醒电路证据。
#include "demo.h"
#include "bsp_display.h"
#include "ui_pixel.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdint.h>

static const char *TAG = "demo_power";

#define LIGHT_SLEEP_TIME_US (2ULL * 1000ULL * 1000ULL)
#define DEEP_SLEEP_TIME_US  (5ULL * 1000ULL * 1000ULL)
#define DEEP_SLEEP_MAGIC    0x464F4C4FUL

typedef enum {
    SLEEP_COMMAND_LIGHT = 1,
    SLEEP_COMMAND_DEEP,
} sleep_command_t;

static lv_obj_t *s_scr;
static lv_obj_t *s_status;
static lv_obj_t *s_mode_cards[2];
static lv_obj_t *s_mascot;
static TaskHandle_t s_task;
static volatile bool s_busy;
static int s_selected;
static RTC_DATA_ATTR uint32_t s_deep_sleep_magic;
static RTC_DATA_ATTR uint32_t s_deep_sleep_count;

static void menu_refresh(void)
{
    for (int i = 0; i < 2; i++) {
        ui_pixel_set_selected(s_mode_cards[i], i == s_selected, true);
    }
}

static void set_status(const char *text)
{
    if (!bsp_lvgl_lock(500)) return;
    if (s_status) lv_label_set_text(s_status, text);
    bsp_lvgl_unlock();
}

static void sleep_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t command = 0;
        xTaskNotifyWait(0, UINT32_MAX, &command, portMAX_DELAY);
        if (!s_scr) continue;

        s_busy = true;
        if (command == SLEEP_COMMAND_DEEP) {
            set_status("DEEP SLEEP: 5 SEC\nApplication will restart");
            vTaskDelay(pdMS_TO_TICKS(250));
            esp_err_t err = esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_US);
            if (err == ESP_OK) {
                if (s_deep_sleep_magic != DEEP_SLEEP_MAGIC) s_deep_sleep_count = 0;
                s_deep_sleep_magic = DEEP_SLEEP_MAGIC;
                s_deep_sleep_count++;
                bsp_display_backlight(0);
                esp_deep_sleep_start();
            }
            char text[96];
            snprintf(text, sizeof(text), "Deep sleep failed:\n%s", esp_err_to_name(err));
            set_status(text);
            ESP_LOGE(TAG, "deep sleep 失败: %s", esp_err_to_name(err));
        } else {
            set_status("LIGHT SLEEP: 2 SEC\nTimer wakeup");
            vTaskDelay(pdMS_TO_TICKS(150));
            bsp_display_backlight(0);

            esp_err_t err = esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_TIME_US);
            int64_t before = esp_timer_get_time();
            if (err == ESP_OK) err = esp_light_sleep_start();
            int64_t slept_ms = (esp_timer_get_time() - before) / 1000;
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            bsp_display_backlight(100);

            char text[128];
            if (err == ESP_OK) {
                snprintf(text, sizeof(text), "LIGHT WAKE: TIMER\nSlept: %lld ms",
                         (long long)slept_ms);
            } else {
                snprintf(text, sizeof(text), "Light sleep failed:\n%s", esp_err_to_name(err));
                ESP_LOGE(TAG, "light sleep 失败: %s", esp_err_to_name(err));
            }
            set_status(text);
        }
        s_busy = false;
    }
}

void demo_low_power_enter(void)
{
    s_scr = ui_pixel_screen_create("LOW POWER");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 14, 54, 212, 190, UI_PAPER);
    s_status = lv_label_create(panel);
    lv_obj_set_width(s_status, 184);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 1);
    if (s_deep_sleep_magic == DEEP_SLEEP_MAGIC &&
        esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        lv_label_set_text_fmt(s_status,
                              "DEEP TIMER WAKE  #%lu\nUP/DOWN: SELECT  OK: RUN",
                              (unsigned long)s_deep_sleep_count);
    } else {
        lv_label_set_text(s_status, "UP/DOWN: SELECT  OK: RUN\nRTC TIMER WAKE ONLY");
    }

    static const char *MODE_NAMES[] = {
        "LIGHT SLEEP  |  2 SEC",
        "DEEP SLEEP   |  5 SEC",
    };
    for (int i = 0; i < 2; i++) {
        s_mode_cards[i] = ui_pixel_panel_create(panel, 7, 56 + i * 54,
                                                 176, 42, UI_PAPER);
        lv_obj_t *label = lv_label_create(s_mode_cards[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(UI_INK), 0);
        lv_label_set_text(label, MODE_NAMES[i]);
        lv_obj_center(label);
    }
    s_selected = 0;
    menu_refresh();
    s_mascot = ui_pixel_mascot_create(s_scr, 101, 246);
    s_busy = false;
    if (!s_task && xTaskCreate(sleep_task, "demo_sleep", 3072, NULL, 4, &s_task) != pdPASS) {
        lv_label_set_text(s_status, "Cannot create\nsleep worker");
        ESP_LOGE(TAG, "创建 light-sleep 任务失败");
    }
    lv_screen_load(s_scr);
}

void demo_low_power_exit(void)
{
    if (s_task) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    s_busy = false;
    bsp_display_backlight(100);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_status = NULL;
        s_mode_cards[0] = s_mode_cards[1] = NULL;
        s_mascot = NULL;
    }
}

void demo_low_power_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK || s_busy || !s_task) return;
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        s_selected = (s_selected + 1) % 2;
        menu_refresh();
        ui_pixel_mascot_jump(s_mascot);
    } else if (btn == BSP_BTN_OK) {
        uint32_t command = s_selected == 0 ? SLEEP_COMMAND_LIGHT : SLEEP_COMMAND_DEEP;
        xTaskNotify(s_task, command, eSetValueWithOverwrite);
    }
}
