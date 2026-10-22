// main/main.c —— FoloToy AI Passport · WorkBuddy 硬件伴侣（独立应用入口）
//
// 二次开发应用：开机直接进入 WorkBuddy 伴侣页（main/workbuddy_companion.c），
// 不再使用基线 demo 测试菜单（符合二次开发 UI 强制重新设计规则）。
// 仅做 BSP 初始化 + 按键派发，应用 UI / 状态机 / BLE 全部在 companion 内。
#include "bsp_i2c.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "workbuddy_companion.h"
#include "key_guard.h"
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

static const char *TAG = "main";

#define INPUT_QUEUE_DEPTH 8
typedef struct { bsp_btn_t btn; bsp_btn_ev_t event; } input_event_t;

static QueueHandle_t s_input_queue;
static TaskHandle_t  s_input_task;
static volatile bool s_input_ready;

// 唤醒键长按误触发防护（深睡唤醒时那颗键仍按住）
#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)
static key_guard_t s_key_guard;

static void input_task(void *arg) {
    (void)arg;
    input_event_t input;
    for (;;) {
        if (xQueueReceive(s_input_queue, &input, portMAX_DELAY) == pdTRUE) {
            workbuddy_companion_key(input.btn, input.event);
        }
    }
}

// button 回调运行在共享 esp_timer 任务，只入队并返回
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (key_guard_consume(&s_key_guard, ev == BSP_BTN_RELEASE, esp_timer_get_time())) return;
    if (!s_input_ready || !s_input_queue) return;
    const input_event_t input = { .btn = btn, .event = ev };
    (void)xQueueSend(s_input_queue, &input, 0);
}

void app_main(void) {
    ESP_LOGI(TAG, "WorkBuddy 伴侣启动");

    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & (1u << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
        ESP_LOGI(TAG, "按键唤醒:先屏蔽按键动作,直到那颗键松手");
    }

    bsp_i2c_init();
    bsp_i2c_scan();

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败。检查 SPI 接线(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(input_event_t));
    if (!s_input_queue) { ESP_LOGE(TAG, "输入队列创建失败"); return; }
    if (xTaskCreate(input_task, "app_input", 4096, NULL, 5, &s_input_task) != pdPASS) {
        vQueueDelete(s_input_queue); s_input_queue = NULL;
        ESP_LOGE(TAG, "输入任务创建失败"); return;
    }

    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败");
        vQueueDelete(s_input_queue); s_input_queue = NULL;
        return;
    }
    if (s_key_guard.armed) {
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_key_guard, released);
    }

    // 音频 / 电量按需初始化（companion 内部也会按需 init 音频）
    (void)bsp_audio_init();
    (void)bsp_battery_init();

    // 进入应用页（持 LVGL 锁）
    if (bsp_lvgl_lock(1000)) {
        workbuddy_companion_enter();
        bsp_lvgl_unlock();
        s_input_ready = true;
    }

    // 启动伴侣服务（USB-CDC 串口链路）
    esp_err_t e = workbuddy_companion_start();
    if (e != ESP_OK) ESP_LOGE(TAG, "伴侣启动失败: %s", esp_err_to_name(e));

    ESP_LOGI(TAG, "就绪: 直接进入 WorkBuddy 伴侣页");
}
