// main/main.c — FoloToy AI Passport "Mini World" application entry.
// Boots straight into the 2D tile sandbox (no demo test menu). Owns input
// dispatch; the world screen owns its tick/timer/teardown.
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "mini_common.h"
#include "mini_sfx.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "mini";

static QueueHandle_t s_input_queue;
static TaskHandle_t s_dispatch_task;
static volatile bool s_dispatch_ready;

static void process_input(const mini_input_t *in) {
    if (!bsp_lvgl_lock(500)) {
        return;
    }
    mini_world_key(in);
    bsp_lvgl_unlock();
}

static void dispatch_task(void *arg) {
    (void)arg;
    mini_input_t in;
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
    const mini_input_t in = { .btn = btn, .ev = ev };
    (void)xQueueSend(s_input_queue, &in, 0);
}

static esp_err_t dispatch_init(void) {
    s_input_queue = xQueueCreate(8, sizeof(mini_input_t));
    if (!s_input_queue) {
        return ESP_ERR_NO_MEM;
    }
    if (xTaskCreate(dispatch_task, "mini_in", 4096, NULL, 5,
                    &s_dispatch_task) != pdPASS) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void app_main(void) {
    ESP_LOGI(TAG, "Mini World starting");

    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL init failed: "
                      "SPI MOSI=%d SCLK=%d CS=%d DC=%d BL=%d",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    bool audio_ok = (bsp_audio_init() == ESP_OK);
    if (audio_ok) {
        (void)mini_sfx_init();
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

    if (bsp_lvgl_lock(1000)) {
        mini_world_enter();
        bsp_lvgl_unlock();
        s_dispatch_ready = true;
    }
    ESP_LOGI(TAG, "ready: audio=%d battery=%d", audio_ok, battery_ok);
}
