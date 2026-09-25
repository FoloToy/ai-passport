// main/main.c — FoloToy AI Passport "Eggy Party" application entry.
// Boots straight into the party hub (no demo test menu). Owns the screen table,
// input dispatch, and idle deep-sleep supervisor.
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "eggy_common.h"
#include "eggy_sfx.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "eggy";

// ---- Screen table (declared per-file) ----
void eggy_hub_enter(void);  void eggy_hub_exit(void);  void eggy_hub_key(const eggy_input_t *);
void eggy_run_enter(void); void eggy_run_exit(void); void eggy_run_key(const eggy_input_t *);
void eggy_tap_enter(void); void eggy_tap_exit(void); void eggy_tap_key(const eggy_input_t *);
void eggy_spin_enter(void); void eggy_spin_exit(void); void eggy_spin_key(const eggy_input_t *);

static const eggy_screen_t SCREENS[] = {
    { EGGY_SCREEN_HUB,  eggy_hub_enter,  eggy_hub_exit,  eggy_hub_key  },
    { EGGY_SCREEN_RUN,  eggy_run_enter,  eggy_run_exit,  eggy_run_key  },
    { EGGY_SCREEN_TAP,  eggy_tap_enter,  eggy_tap_exit,  eggy_tap_key  },
    { EGGY_SCREEN_SPIN, eggy_spin_enter, eggy_spin_exit, eggy_spin_key },
};
#define SCREEN_COUNT (sizeof(SCREENS) / sizeof(SCREENS[0]))

static const eggy_screen_t *s_active;
static eggy_screen_id_t s_active_id = EGGY_SCREEN_HUB;

static QueueHandle_t s_input_queue;
static TaskHandle_t s_dispatch_task;
static volatile bool s_dispatch_ready;

// Caller MUST hold the LVGL lock (or be the LVGL task).
void eggy_switch_to(eggy_screen_id_t id) {
    if ((unsigned)id >= SCREEN_COUNT) {
        return;
    }
    if (s_active) {
        s_active->exit();
    }
    s_active = &SCREENS[id];
    s_active_id = id;
    s_active->enter();
}

static void process_input(const eggy_input_t *in) {
    eggy_idle_notify_activity();
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    // OK-long is the universal "back to hub" gesture (never on the hub itself).
    if (in->btn == BSP_BTN_OK && in->ev == BSP_BTN_LONG &&
        s_active_id != EGGY_SCREEN_HUB) {
        eggy_sfx_play(EGGY_SFX_BACK);
        eggy_switch_to(EGGY_SCREEN_HUB);
    } else if (s_active) {
        s_active->key(in);
    }
    bsp_lvgl_unlock();
}

static void dispatch_task(void *arg) {
    (void)arg;
    eggy_input_t in;
    for (;;) {
        if (xQueueReceive(s_input_queue, &in, portMAX_DELAY) == pdTRUE) {
            process_input(&in);
        }
    }
}

// Button callback runs on the shared esp_timer task: enqueue only.
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (!s_dispatch_ready || !s_input_queue) {
        return;
    }
    const eggy_input_t in = { .btn = btn, .ev = ev };
    (void)xQueueSend(s_input_queue, &in, 0);
}

static esp_err_t dispatch_init(void) {
    s_input_queue = xQueueCreate(8, sizeof(eggy_input_t));
    if (!s_input_queue) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(dispatch_task, "eggy_in", 4096, NULL, 5,
                    &s_dispatch_task) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "Eggy Party starting");
    if (eggy_slept_before()) {
        ESP_LOGI(TAG, "woke from eggy-party deep sleep");
    } else {
        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause != ESP_SLEEP_WAKEUP_UNDEFINED) {
            ESP_LOGI(TAG, "wakeup cause: %d", (int)cause);
        }
    }

    bsp_i2c_init();
    bsp_i2c_scan();

    // The screen is the UI carrier; without it there is no app.
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed: "
                      "SPI MOSI=%d SCLK=%d CS=%d DC=%d BL=%d",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool audio_ok = (bsp_audio_init() == ESP_OK);
    if (audio_ok) {
        (void)eggy_sfx_init();
    }
    bool battery_ok = (bsp_battery_init() == ESP_OK);
    (void)battery_ok;

    if (dispatch_init() != ESP_OK) {
        ESP_LOGE(TAG, "input dispatch task creation failed");
        return;
    }
    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "button init failed");
        return;
    }

    eggy_sleep_supervisor_start();

    if (bsp_lvgl_lock(1000)) {
        eggy_switch_to(EGGY_SCREEN_HUB);
        bsp_lvgl_unlock();
        s_dispatch_ready = true;
    }
    ESP_LOGI(TAG, "ready: audio=%d battery=%d", audio_ok, battery_ok);
}
