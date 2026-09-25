// eggy_sleep.c — idle supervisor + deep-sleep teardown for the eggy party.
// Mirrors the proven baseline sequence from main/demo_low_power.c and arms a
// low-level GPIO wake on the shared button pin (ADC must be released first,
// per docs/reference/shinku-chen/landscape-rotation-and-deep-sleep-key-wake.md).
#include "eggy_common.h"

#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

static const char *TAG = "eggy_sleep";

#define EGGY_SLEEP_MAGIC 0xE66200D5u
static RTC_DATA_ATTR uint32_t s_wake_magic;

static esp_timer_handle_t s_timer;
static volatile int64_t s_last_activity_us;
static volatile bool s_in_game;
static volatile bool s_sleeping;

static int64_t now_us(void) {
    return esp_timer_get_time();
}

void eggy_idle_notify_activity(void) {
    s_last_activity_us = now_us();
}

void eggy_sleep_set_in_game(bool in_game) {
    s_in_game = in_game;
    eggy_idle_notify_activity();
}

static void show_sleeping_screen(void) {
    // Brief on-screen notice so the user sees the device go to sleep.
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    lv_obj_t *scr = lv_screen_active();
    lv_obj_t *label = lv_label_create(scr);
    lv_label_set_text(label, "SLEEPING");
    lv_obj_set_style_text_color(label, lv_color_hex(EGGY_COLOR_DIM), 0);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_20, 0);
    lv_obj_align(label, LV_ALIGN_CENTER, 0, 0);
    bsp_lvgl_unlock();
}

static void do_deep_sleep(void) {
    s_sleeping = true;
    ESP_LOGI(TAG, "idle timeout, entering deep sleep");

    // Release the ADC that owns GPIO0 and re-arm a low-level GPIO wake. If a
    // key is still held (level 0) we abort rather than sleep-wake-loop.
    int level = bsp_button_prepare_deep_sleep();
    if (level == 0) {
        ESP_LOGW(TAG, "key held at sleep time; aborting deep sleep");
        s_sleeping = false;
        eggy_idle_notify_activity();
        return;
    }
    esp_err_t err = esp_deep_sleep_enable_gpio_wakeup(
        (1ULL << BSP_BTN_GPIO), ESP_GPIO_WAKEUP_GPIO_LOW);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "gpio wake arm failed: %s", esp_err_to_name(err));
        esp_restart();  // peripherals half-torn-down; safest to restart
    }

    // CW2017 + ES8311 share I2C; finish battery/codec writes before freeing pins.
    (void)bsp_battery_sleep();
    (void)bsp_audio_sleep();
    (void)bsp_audio_prepare_deep_sleep();
    (void)bsp_i2c_prepare_deep_sleep();

    show_sleeping_screen();
    vTaskDelay(pdMS_TO_TICKS(400));

    // Block LVGL from flushing into a powered-down panel.
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "could not stop LVGL flush before sleep, restarting");
        esp_restart();
    }
    (void)bsp_display_prepare_deep_sleep();

    s_wake_magic = EGGY_SLEEP_MAGIC;
    esp_deep_sleep_start();
    // Unreachable; if it returns, peripherals are torn down beyond recovery.
    ESP_LOGE(TAG, "esp_deep_sleep_start returned unexpectedly, restarting");
    esp_restart();
}

static void timer_callback(void *arg) {
    (void)arg;
    if (s_sleeping) {
        return;
    }
    int64_t threshold_us = (int64_t)(s_in_game ? EGGY_IDLE_SLEEP_GAME_MS
                                                : EGGY_IDLE_SLEEP_HUB_MS) * 1000;
    if (now_us() - s_last_activity_us >= threshold_us) {
        do_deep_sleep();
    }
}

void eggy_sleep_supervisor_start(void) {
    if (s_timer) {
        return;
    }
    eggy_idle_notify_activity();
    const esp_timer_create_args_t args = {
        .callback = timer_callback,
        .name = "eggy_idle",
    };
    if (esp_timer_create(&args, &s_timer) != ESP_OK) {
        ESP_LOGE(TAG, "could not create idle timer");
        return;
    }
    // 1 Hz poll is plenty for a 60s+ timeout.
    (void)esp_timer_start_periodic(s_timer, 1000 * 1000);
}

bool eggy_slept_before(void) {
    return s_wake_magic == EGGY_SLEEP_MAGIC;
}
