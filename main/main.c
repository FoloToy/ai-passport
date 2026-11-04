#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "key_guard.h"
#include "radio_nvs.h"
#include "radio_player.h"
#include "radio_stations.h"
#include "radio_ui.h"
#include "radio_wifi.h"

#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "global_radio";

#define INPUT_QUEUE_DEPTH 8
#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue;
static volatile bool s_input_ready;
static key_guard_t s_key_guard;
static radio_settings_t s_settings;

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (s_key_guard.armed) {
        if (key_guard_consume(&s_key_guard, ev == BSP_BTN_RELEASE, esp_timer_get_time())) {
            return;
        }
    }
    if (!s_input_ready || !s_input_queue) {
        return;
    }
    input_event_t input = {.btn = btn, .event = ev};
    (void)xQueueSend(s_input_queue, &input, 0);
}

static void input_task(void *arg)
{
    (void)arg;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) == pdTRUE) {
            radio_ui_on_key(input.btn, input.event);
        }
    }
}

static void wifi_cb(radio_wifi_state_t state, const char *detail, void *user)
{
    (void)user;
    radio_ui_on_wifi(state, detail);
}

static void player_cb(radio_player_state_t state, const char *message, void *user)
{
    (void)user;
    radio_ui_on_player(state, message);
}

void app_main(void)
{
    ESP_LOGI(TAG, "全球广播 0.1 启动 stations=%u", (unsigned)radio_station_count());

    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & ESP_SLEEP_WAKEUP_GPIO) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
    }

    ESP_ERROR_CHECK(bsp_i2c_init());
    ESP_ERROR_CHECK(bsp_display_init());
    ESP_ERROR_CHECK(bsp_battery_init());

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    ESP_ERROR_CHECK(s_input_queue ? ESP_OK : ESP_ERR_NO_MEM);
    xTaskCreate(input_task, "radio_input", 3072, NULL, 6, NULL);

    ESP_ERROR_CHECK(bsp_button_init(on_key, NULL));
    if (s_key_guard.armed) {
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_key_guard, released);
    }

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);
    radio_nvs_load(&s_settings);

    ESP_ERROR_CHECK(radio_ui_start(&s_settings));
    s_input_ready = true;

    ESP_ERROR_CHECK(radio_wifi_init(wifi_cb, NULL));
    ESP_ERROR_CHECK(radio_player_init(player_cb, NULL));
    radio_player_set_volume(s_settings.volume);

    if (s_settings.has_wifi) {
        if (radio_wifi_connect_saved(&s_settings) == ESP_OK) {
            int idx = radio_station_find_id(s_settings.station_id);
            const radio_station_t *st = radio_station_at(idx >= 0 ? (size_t)idx : 0);
            if (st) {
                radio_player_play_url(st->url);
            }
        } else {
            ESP_LOGW(TAG, "saved Wi-Fi failed; starting SoftAP");
            ESP_ERROR_CHECK(radio_wifi_start_softap());
        }
    } else {
        ESP_ERROR_CHECK(radio_wifi_start_softap());
    }
}
