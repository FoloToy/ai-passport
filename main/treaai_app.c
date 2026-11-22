// main/treaai_app.c — TreaAIPassport 应用状态机。
//
// 职责:
//   1. 初始化 BSP / TreaAI / UI / 按键回调
//   2. 把按键事件转成 AI 请求 (文本 / 音频)
//   3. 音频 worker task: 从 ES8311 读 PCM → 累计 → 交给 treaai_send_audio
//   4. AI 回复回调: 转发到 UI 显示, 可选播放音频
//   5. Deep-sleep key wake guard: 按键唤醒后先吞事件直到松手 (硬件约束)
#include "treaai_app.h"
#include "treaai_ui.h"
#include "treaai.h"
#include "key_guard.h"
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "treaai_app";

#define KEY_GUARD_MAX_US (10LL * 1000 * 1000)

// ---------------------------------------------------------------------------
// 状态
// ---------------------------------------------------------------------------

typedef enum {
    APP_RECORD_IDLE = 0,
    APP_RECORDING,
} app_record_state_t;

typedef struct {
    QueueHandle_t input_queue;
    TaskHandle_t  input_task;
    TaskHandle_t  audio_task;
    volatile app_record_state_t rec_state;
    int64_t       last_battery_tick_us;
    volatile bool input_ready;
    key_guard_t   key_guard;
} app_ctx_t;

static app_ctx_t s_app;

// ---------------------------------------------------------------------------
// AI 回复回调 (treaai worker context)
// ---------------------------------------------------------------------------

static void on_ai_reply(const char *text, int audio_len, void *user)
{
    (void)user;
    ESP_LOGI(TAG, "AI reply (audio=%d): %s", audio_len, text ? text : "(null)");
    if (text) treaai_ui_on_ai_reply(text);
}

// ---------------------------------------------------------------------------
// 按键回调 (esp_timer task)
//
// Contract (tested by tests/test_key_wake_contract.py):
//   key_guard_consume() MUST run BEFORE the readiness check. A release can
//   arrive after the post-init voltage sample but before input dispatch is
//   ready; dropping it here would leave the guard armed to swallow the next
//   real press.
// ---------------------------------------------------------------------------

typedef struct {
    bsp_btn_t   btn;
    bsp_btn_ev_t ev;
} app_input_t;

static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    // Consume the guard BEFORE the readiness check.
    const bool was_armed = s_app.key_guard.armed;
    if (key_guard_consume(&s_app.key_guard, ev == BSP_BTN_RELEASE, esp_timer_get_time())) {
        if (was_armed && !s_app.key_guard.armed) {
            ESP_LOGI(TAG, "唤醒键已松手,按键恢复响应");
        }
        return;
    }
    if (!s_app.input_ready || !s_app.input_queue) return;
    const app_input_t in = { .btn = btn, .ev = ev };
    (void)xQueueSend(s_app.input_queue, &in, 0);
}

// ---------------------------------------------------------------------------
// 音频 worker task
// ---------------------------------------------------------------------------

static void audio_task(void *arg)
{
    (void)arg;
    esp_err_t fmt_ok = bsp_audio_set_format(16000, 16, 1);
    if (fmt_ok != ESP_OK) {
        ESP_LOGW(TAG, "audio format 16k16mono failed: %s", esp_err_to_name(fmt_ok));
    }
    const int CHUNK_SAMPLES = 320; // 20ms @ 16kHz
    int16_t chunk[CHUNK_SAMPLES];

    for (;;) {
        if (s_app.rec_state != APP_RECORDING) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        esp_err_t e = bsp_audio_read(chunk, CHUNK_SAMPLES * sizeof(int16_t));
        if (e == ESP_OK) {
            treaai_upload_audio_chunk(chunk, CHUNK_SAMPLES);
        } else {
            ESP_LOGW(TAG, "audio read: %s", esp_err_to_name(e));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

// ---------------------------------------------------------------------------
// 输入 task
// ---------------------------------------------------------------------------

static void input_task(void *arg)
{
    (void)arg;
    app_input_t in;
    for (;;) {
        if (xQueueReceive(s_app.input_queue, &in, portMAX_DELAY) != pdTRUE) continue;

        treaai_ui_on_key(in.btn, in.ev);

        if (treaai_ui_current() == UI_SCR_CHAT) {
            if (in.btn == BSP_BTN_OK) {
                if (in.ev == BSP_BTN_LONG) {
                    ESP_LOGI(TAG, "START recording");
                    s_app.rec_state = APP_RECORDING;
                    bsp_audio_wake();
                    treaai_ui_on_state_change(TREAAI_STATE_LISTENING);
                } else if (in.ev == BSP_BTN_RELEASE && s_app.rec_state == APP_RECORDING) {
                    ESP_LOGI(TAG, "STOP recording, send to AI");
                    s_app.rec_state = APP_RECORD_IDLE;
                    esp_err_t e = treaai_send_audio(on_ai_reply, NULL);
                    if (e != ESP_OK) {
                        ESP_LOGW(TAG, "send_audio: %s", esp_err_to_name(e));
                        treaai_ui_on_state_change(TREAAI_STATE_ERROR);
                    }
                }
            }
        }
    }
}

// ---------------------------------------------------------------------------
// App 公开入口
// ---------------------------------------------------------------------------

esp_err_t treaai_app_init(void)
{
    // 1. Deep-sleep key wake guard — 按键唤醒时那颗键还按着, 先吞掉它
    const uint32_t wake_causes = esp_sleep_get_wakeup_causes();
    if (wake_causes & (1u << ESP_SLEEP_WAKEUP_GPIO)) {
        key_guard_arm(&s_app.key_guard, esp_timer_get_time(), KEY_GUARD_MAX_US);
        ESP_LOGI(TAG, "按键唤醒:先屏蔽按键动作,直到那颗键松手");
    }

    // 2. BSP: 显示是必须的, 失败直接返回; 其他可降级
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display init failed (check SPI: MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return ESP_ERR_NOT_SUPPORTED;
    }
    bsp_display_backlight(100);

    bsp_audio_init();
    bsp_battery_init();

    // 3. TreaAI
    esp_err_t e = treaai_init();
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "treaai_init: %s", esp_err_to_name(e));
        return e;
    }

    // 4. UI
    treaai_ui_init();

    // 5. 按键 + 输入队列 + worker tasks
    s_app.input_queue = xQueueCreate(16, sizeof(app_input_t));
    if (!s_app.input_queue) return ESP_ERR_NO_MEM;

    if (xTaskCreate(input_task, "treaai_input", 4096, NULL, 5, &s_app.input_task) != pdPASS)
        return ESP_ERR_NO_MEM;
    if (xTaskCreate(audio_task, "treaai_audio", 4096, NULL, 4, &s_app.audio_task) != pdPASS)
        return ESP_ERR_NO_MEM;

    esp_err_t btn_err = bsp_button_init(on_key, NULL);
    if (btn_err != ESP_OK) {
        ESP_LOGE(TAG, "button init: %s", esp_err_to_name(btn_err));
        return btn_err;
    }

    // 若唤醒键在按键驱动启动前就已松手, 组件不会发 release 事件 → 同步撤防
    if (s_app.key_guard.armed) {
        const bool released = bsp_button_read_mv() >= BSP_BTN_MV_RELEASED_MIN;
        key_guard_sync_released(&s_app.key_guard, released);
        if (!s_app.key_guard.armed) {
            ESP_LOGI(TAG, "唤醒键在按键驱动启动前已松手,提前撤防");
        }
    }

    s_app.input_ready = true;

    // 6. 初始状态
    int soc = bsp_battery_soc();
    treaai_ui_update_battery(soc);
    s_app.last_battery_tick_us = esp_timer_get_time();
    treaai_ui_on_state_change(TREAAI_STATE_IDLE);

    ESP_LOGI(TAG, "TreaAIPassport 应用初始化完成");
    return ESP_OK;
}

void treaai_app_tick(void)
{
    int64_t now = esp_timer_get_time();
    if (now - s_app.last_battery_tick_us >= 5000000) {
        int soc = bsp_battery_soc();
        treaai_ui_update_battery(soc);
        s_app.last_battery_tick_us = now;
    }
    treaai_ui_on_state_change(treaai_get_state());
}

void treaai_app_on_quick_select(int idx)
{
    char prompt[128];
    treaai_get_quick_prompt(idx, prompt, sizeof(prompt));
    ESP_LOGI(TAG, "quick[%d]: %s", idx, prompt);
    esp_err_t e = treaai_send_text(prompt, on_ai_reply, NULL);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "send_text: %s", esp_err_to_name(e));
    }
}

void treaai_app_on_chat_record_start(void) { s_app.rec_state = APP_RECORDING; }
void treaai_app_on_chat_record_stop(void)  { s_app.rec_state = APP_RECORD_IDLE; }
