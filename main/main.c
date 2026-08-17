#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_random.h"
#include "tamagezi_app.h"
#include "tamagezi_audio.h"
#include "tamagezi_model.h"
#include "tamagezi_store.h"

static const char *TAG = "tamagezi";

static void on_key(bsp_btn_t button, bsp_btn_ev_t event, void *context)
{
    (void)context;
    tmz_app_button(button, event);
}

void app_main(void)
{
    ESP_LOGI(TAG, "FoloToy AI Passport Tamagezi starting");
    bsp_i2c_init();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display init failed (MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    tmz_model_t model;
    bool store_ok = tmz_store_init();
    if (!store_ok || !tmz_store_load(&model)) {
        tmz_model_init(&model, esp_random());
        if (store_ok) tmz_store_save_async(&model);
    }

    bool battery_ok = bsp_battery_init() == ESP_OK;
    bool audio_ok = tmz_audio_start(model.sound_level);
    bool app_ok = false;
    if (bsp_lvgl_lock(1000)) {
        app_ok = tmz_app_start(&model, battery_ok);
        bsp_lvgl_unlock();
    }
    bool button_ok = app_ok && bsp_button_init(on_key, NULL) == ESP_OK;

    ESP_LOGI(TAG, "ready: app=%d buttons=%d audio=%d battery=%d store=%d",
             app_ok, button_ok, audio_ok, battery_ok, store_ok);
}
