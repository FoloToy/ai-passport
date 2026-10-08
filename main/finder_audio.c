// main/finder_audio.c —— 见 finder_audio.h。
#include "finder_audio.h"

#include "bsp_audio.h"
#include "finder_beep.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"

static const char *TAG = "finder_audio";

#define FINDER_AUDIO_TASK_PRIORITY    5
#define FINDER_AUDIO_TASK_STACK    4096
#define FINDER_AUDIO_VOLUME          70
#define FINDER_AUDIO_IDLE_MS         20
#define FINDER_AUDIO_STOP_TIMEOUT_MS 2000

static SemaphoreHandle_t  s_stopped;
static TaskHandle_t       s_task;
static volatile int32_t   s_level; // int32 保证跨任务单字读写不会撕裂
static volatile bool      s_muted;
static volatile bool      s_stop_requested;
static volatile esp_err_t s_error;
static bool               s_initialized;

static void beep_task(void *arg)
{
    (void)arg;
    finder_beep_gen_t gen;
    finder_beep_init(&gen, FINDER_BEEP_RATE_HZ);

    int16_t chunk[FINDER_AUDIO_CHUNK_SAMPLES];
    bool was_silent = true;

    while (!s_stop_requested) {
        int8_t level = (int8_t)s_level;
        bool muted = s_muted;

        if (muted || level <= 0) {
            // 静音时不向 DMA 送数据：省掉编解码器与 DMA 的持续功耗。
            // 代价是恢复发声最多有 IDLE_MS 的延迟，相对 80ms 级节奏无感。
            was_silent = true;
            vTaskDelay(pdMS_TO_TICKS(FINDER_AUDIO_IDLE_MS));
            continue;
        }

        if (was_silent) {
            // 从静默恢复：重新起手，让第一次蜂鸣从相位 0 并带淡入开始，
            // 而不是接在一段被冻结的旧波形后面——那会是一个咔哒。
            finder_beep_init(&gen, FINDER_BEEP_RATE_HZ);
            was_silent = false;
        }

        // 同等级重复设置不会打断节奏（finder_beep_set 只在等级变化时重启周期）。
        finder_beep_set(&gen, level, false);
        finder_beep_next(&gen, chunk, FINDER_AUDIO_CHUNK_SAMPLES);

        esp_err_t err = bsp_audio_write(chunk, sizeof(chunk));
        if (err != ESP_OK) {
            // 不在坏掉的音频通道上无限重试：记录并退出，让上层能如实报告。
            s_error = err;
            ESP_LOGE(TAG, "bsp_audio_write 失败: %s", esp_err_to_name(err));
            break;
        }
    }

    xSemaphoreGive(s_stopped);
    for (;;) {
        vTaskSuspend(NULL);
    }
}

esp_err_t finder_audio_start(void)
{
    if (s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    s_error = ESP_OK;
    s_stop_requested = false;
    s_level = 0;
    s_muted = false;

    // bsp_audio_init() 由 main 在启动时完成；这里只声明本应用需要的播放格式。
    esp_err_t err = bsp_audio_set_format(FINDER_BEEP_RATE_HZ, 16, 1);
    if (err != ESP_OK) {
        s_error = err;
        return err;
    }
    bsp_audio_set_volume(FINDER_AUDIO_VOLUME);

    s_stopped = xSemaphoreCreateBinary();
    if (!s_stopped) {
        s_error = ESP_ERR_NO_MEM;
        return ESP_ERR_NO_MEM;
    }

    if (xTaskCreate(beep_task, "finder_beep", FINDER_AUDIO_TASK_STACK, NULL,
                    FINDER_AUDIO_TASK_PRIORITY, &s_task) != pdPASS) {
        vSemaphoreDelete(s_stopped);
        s_stopped = NULL;
        s_task = NULL;
        s_error = ESP_ERR_NO_MEM;
        return ESP_ERR_NO_MEM;
    }

    s_initialized = true;
    return ESP_OK;
}

esp_err_t finder_audio_stop(void)
{
    if (!s_initialized) {
        return ESP_OK;
    }

    s_stop_requested = true;
    if (s_stopped &&
        xSemaphoreTake(s_stopped, pdMS_TO_TICKS(FINDER_AUDIO_STOP_TIMEOUT_MS)) != pdTRUE) {
        // 保留所有权以便重试：任务可能还持有音频通道，此时删任务会留下半关闭状态。
        ESP_LOGE(TAG, "等待蜂鸣任务停止超时");
        s_error = ESP_ERR_TIMEOUT;
        return ESP_ERR_TIMEOUT;
    }

    if (s_task) {
        vTaskDelete(s_task);
        s_task = NULL;
    }
    if (s_stopped) {
        vSemaphoreDelete(s_stopped);
        s_stopped = NULL;
    }

    s_initialized = false;
    return s_error;
}

void finder_audio_set(int8_t level, bool muted)
{
    s_level = (int32_t)level;
    s_muted = muted;
}

bool finder_audio_running(void)
{
    return s_initialized;
}

esp_err_t finder_audio_last_error(void)
{
    return s_error;
}
