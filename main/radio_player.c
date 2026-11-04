#include "radio_player.h"

#include "bsp_audio.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mp3dec.h"

#include <stdio.h>
#include <string.h>

static const char *TAG = "radio_player";

#define IN_BUF_SIZE 24576
#define PCM_SAMPLES 2304
#define PLAYER_STACK 8192
#define HTTP_TIMEOUT_MS 15000

typedef struct {
    char url[256];
    bool stop;
    bool pause;
} player_cmd_t;

static HMP3Decoder s_dec;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_lock;
static radio_player_status_cb_t s_cb;
static void *s_cb_user;
static volatile radio_player_state_t s_state = RADIO_PLAYER_IDLE;
static char s_err[96];
static char s_url[256];
static uint8_t s_volume = 60;
static volatile bool s_pause;
static volatile bool s_stop_req;
static volatile bool s_running;

static void set_state(radio_player_state_t st, const char *msg)
{
    s_state = st;
    if (msg) {
        snprintf(s_err, sizeof(s_err), "%s", msg);
    } else if (st != RADIO_PLAYER_ERROR) {
        s_err[0] = '\0';
    }
    if (s_cb) {
        s_cb(st, msg, s_cb_user);
    }
}

static void mix_to_mono(int16_t *pcm, int samples, int channels)
{
    if (channels <= 1) {
        return;
    }
    for (int i = 0; i < samples; i++) {
        int32_t l = pcm[i * 2];
        int32_t r = pcm[i * 2 + 1];
        pcm[i] = (int16_t)((l + r) / 2);
    }
}

static int refill(esp_http_client_handle_t client, uint8_t *buf, int *filled, int capacity)
{
    if (*filled >= capacity) {
        return 0;
    }
    int want = capacity - *filled;
    int n = esp_http_client_read(client, (char *)buf + *filled, want);
    if (n > 0) {
        *filled += n;
        return n;
    }
    return n; /* 0 = EOF, <0 = error */
}

static void player_task(void *arg)
{
    (void)arg;
    uint8_t *in_buf = heap_caps_malloc(IN_BUF_SIZE, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    int16_t *pcm = heap_caps_malloc(PCM_SAMPLES * 2 * sizeof(int16_t),
                                    MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!in_buf || !pcm || !s_dec) {
        set_state(RADIO_PLAYER_ERROR, "内存不足");
        free(in_buf);
        free(pcm);
        s_running = false;
        vTaskDelete(NULL);
        return;
    }

    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        if (s_stop_req && s_url[0] == '\0') {
            continue;
        }

        char url[256];
        xSemaphoreTake(s_lock, portMAX_DELAY);
        strncpy(url, s_url, sizeof(url) - 1);
        url[sizeof(url) - 1] = '\0';
        s_stop_req = false;
        s_pause = false;
        xSemaphoreGive(s_lock);

        if (!url[0]) {
            continue;
        }

        set_state(RADIO_PLAYER_CONNECTING, "连接中…");
        ESP_LOGI(TAG, "play %s", url);

        esp_http_client_config_t cfg = {
            .url = url,
            .timeout_ms = HTTP_TIMEOUT_MS,
            .buffer_size = 2048,
            .buffer_size_tx = 512,
            .method = HTTP_METHOD_GET,
            .user_agent = "GlobalRadio/0.1 (AI-Passport)",
            .keep_alive_enable = true,
        };
        esp_http_client_handle_t client = esp_http_client_init(&cfg);
        if (!client) {
            set_state(RADIO_PLAYER_ERROR, "无法创建连接");
            continue;
        }
        esp_http_client_set_header(client, "Icy-MetaData", "0");
        esp_http_client_set_header(client, "Accept", "*/*");
        esp_http_client_set_header(client, "Connection", "keep-alive");

        esp_err_t err = esp_http_client_open(client, 0);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "open: %s", esp_err_to_name(err));
            esp_http_client_cleanup(client);
            set_state(RADIO_PLAYER_ERROR, "网络连接失败");
            continue;
        }
        int content_length = esp_http_client_fetch_headers(client);
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP %d len=%d heap=%u", status, content_length,
                 (unsigned)esp_get_free_heap_size());
        if (status < 200 || status >= 300) {
            esp_http_client_close(client);
            esp_http_client_cleanup(client);
            snprintf(s_err, sizeof(s_err), "电台拒绝(%d)", status);
            set_state(RADIO_PLAYER_ERROR, s_err);
            continue;
        }

        set_state(RADIO_PLAYER_BUFFERING, "缓冲中…");
        int filled = 0;
        bool format_set = false;
        int consecutive_fail = 0;

        while (!s_stop_req) {
            while (s_pause && !s_stop_req) {
                set_state(RADIO_PLAYER_PAUSED, "已暂停");
                vTaskDelay(pdMS_TO_TICKS(50));
            }
            if (s_stop_req) {
                break;
            }

            if (filled < 2048) {
                int n = refill(client, in_buf, &filled, IN_BUF_SIZE);
                if (n < 0) {
                    consecutive_fail++;
                    if (consecutive_fail > 8) {
                        set_state(RADIO_PLAYER_ERROR, "读流出错");
                        break;
                    }
                    vTaskDelay(pdMS_TO_TICKS(20));
                    continue;
                }
                if (n == 0 && filled < 128) {
                    set_state(RADIO_PLAYER_ERROR, "流已结束");
                    break;
                }
                consecutive_fail = 0;
            }

            int offset = MP3FindSyncWord(in_buf, filled);
            if (offset < 0) {
                filled = 0;
                continue;
            }
            if (offset > 0) {
                memmove(in_buf, in_buf + offset, filled - offset);
                filled -= offset;
            }

            unsigned char *in_ptr = in_buf;
            int bytes_left = filled;
            int ret = MP3Decode(s_dec, &in_ptr, &bytes_left, pcm, 0);
            int consumed = filled - bytes_left;
            if (consumed > 0) {
                memmove(in_buf, in_buf + consumed, bytes_left);
                filled = bytes_left;
            }

            if (ret != 0) {
                /* Skip one byte and resync on transient errors. */
                if (filled > 1) {
                    memmove(in_buf, in_buf + 1, filled - 1);
                    filled -= 1;
                } else {
                    filled = 0;
                }
                continue;
            }

            MP3FrameInfo info;
            MP3GetLastFrameInfo(s_dec, &info);
            if (info.samprate <= 0 || info.nChans <= 0 || info.outputSamps <= 0) {
                continue;
            }

            if (!format_set) {
                err = bsp_audio_set_format((uint32_t)info.samprate, 16, 1);
                if (err != ESP_OK) {
                    set_state(RADIO_PLAYER_ERROR, "音频设备失败");
                    break;
                }
                bsp_audio_set_volume(s_volume);
                format_set = true;
                set_state(RADIO_PLAYER_PLAYING, "播放中");
            }

            int samples = info.outputSamps / info.nChans;
            mix_to_mono(pcm, samples, info.nChans);
            err = bsp_audio_write(pcm, (size_t)samples * sizeof(int16_t));
            if (err != ESP_OK) {
                ESP_LOGW(TAG, "audio write: %s", esp_err_to_name(err));
            }

            if (filled < 4096) {
                (void)refill(client, in_buf, &filled, IN_BUF_SIZE);
            }
        }

        esp_http_client_close(client);
        esp_http_client_cleanup(client);
        if (s_state != RADIO_PLAYER_ERROR) {
            set_state(RADIO_PLAYER_IDLE, "已停止");
        }
    }
}

esp_err_t radio_player_init(radio_player_status_cb_t cb, void *user)
{
    s_cb = cb;
    s_cb_user = user;
    s_lock = xSemaphoreCreateMutex();
    if (!s_lock) {
        return ESP_ERR_NO_MEM;
    }
    s_dec = MP3InitDecoder();
    if (!s_dec) {
        return ESP_ERR_NO_MEM;
    }
    esp_err_t err = bsp_audio_init();
    if (err != ESP_OK) {
        return err;
    }
    bsp_audio_set_volume(s_volume);
    s_running = true;
    BaseType_t ok = xTaskCreatePinnedToCore(player_task, "radio_mp3", PLAYER_STACK,
                                            NULL, 5, &s_task, 0);
    return ok == pdPASS ? ESP_OK : ESP_ERR_NO_MEM;
}

void radio_player_set_volume(uint8_t percent)
{
    s_volume = percent > 100 ? 100 : percent;
    bsp_audio_set_volume(s_volume);
}

esp_err_t radio_player_play_url(const char *url)
{
    if (!url || !url[0] || !s_task) {
        return ESP_ERR_INVALID_ARG;
    }
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_stop_req = true;
    strncpy(s_url, url, sizeof(s_url) - 1);
    s_url[sizeof(s_url) - 1] = '\0';
    s_pause = false;
    xSemaphoreGive(s_lock);
    /* Allow current loop to notice stop, then kick. */
    vTaskDelay(pdMS_TO_TICKS(30));
    s_stop_req = false;
    xTaskNotifyGive(s_task);
    return ESP_OK;
}

void radio_player_pause(void)
{
    s_pause = true;
}

void radio_player_resume(void)
{
    s_pause = false;
    if (s_state == RADIO_PLAYER_PAUSED) {
        set_state(RADIO_PLAYER_PLAYING, "播放中");
    }
}

void radio_player_stop(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    s_stop_req = true;
    s_url[0] = '\0';
    s_pause = false;
    xSemaphoreGive(s_lock);
}

radio_player_state_t radio_player_state(void)
{
    return s_state;
}

const char *radio_player_last_error(void)
{
    return s_err;
}
