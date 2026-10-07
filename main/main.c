#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "eevee_app.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "main";

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t event;
} input_event_t;

static QueueHandle_t s_input_queue = NULL;
static TaskHandle_t s_input_task = NULL;
static volatile bool s_input_ready = false;

static void input_task(void *pvParam)
{
    (void)pvParam;
    input_event_t evt;
    for (;;) {
        if (xQueueReceive(s_input_queue, &evt, portMAX_DELAY) == pdPASS) {
            eevee_app_handle_button(evt.btn, evt.event);
        }
    }
}

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = { .btn = btn, .event = ev };
    xQueueSend(s_input_queue, &input, 0);
}

void app_main(void)
{
    ESP_LOGI(TAG, "Eevee Smart Badge Starting...");
    esp_sleep_wakeup_cause_t wakeup = esp_sleep_get_wakeup_cause();
    if (wakeup != ESP_SLEEP_WAKEUP_UNDEFINED) {
        ESP_LOGI(TAG, "Wakeup cause: %d", wakeup);
    }

    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 2. 初始化 I2C 与电源外设
    bsp_i2c_init();
    bsp_battery_init();

    // 3. 初始化 LCD 屏幕与 LVGL 9
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display/LVGL init failed!");
        return;
    }
    bsp_display_backlight(100);

    // 4. 初始化按键驱动与事件队列
    s_input_queue = xQueueCreate(8, sizeof(input_event_t));
    if (s_input_queue) {
        xTaskCreate(input_task, "btn_dispatch", 2048, NULL, 10, &s_input_task);
    }
    bsp_button_init(on_key, NULL);
    s_input_ready = true;

    // 5. 启动智能工牌业务应用
    eevee_app_start();

    ESP_LOGI(TAG, "Eevee Smart Badge initialized successfully.");
}
