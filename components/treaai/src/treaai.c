// components/treaai/src/treaai.c
// TreaAI Passport 客户端: NVS 配置持久化 + HTTP 发送 + base64 音频缓冲。
//
// 设计约束:
//   - ESP32-C3 无 PSRAM, 单请求 payload ≤ 8KB (HTTP client 内部缓冲限制)
//   - 录音分片累计: treaai_upload_audio_chunk() 内部 malloc 追加;
//     累计上限 TREAAI_AUDIO_MAX_BYTES, 超限自动截断并打日志
//   - HTTP 请求在独立 FreeRTOS task 执行, 不阻塞 BSP 音频/显示
#include "treaai.h"
#include "treaai_protocol.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "mbedtls/base64.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "treaai";

// 单次请求音频缓冲上限 (base64 编码后 ~12KB → HTTP body ≤ 8KB 需要更紧)
// 16bit 16kHz mono → 每秒 32KB PCM → base64 后 ~43KB → 太大会顶爆 HTTP 缓冲
// 限制累计 ≤ 3 秒录音 = 96KB PCM / 128KB base64 → 分批 HTTP multipart 是另一选项
// 简化版: 限制 ≤ 2 秒 = 64KB PCM, base64 后 ~86KB, 作为 URL POST body
#define TREAAI_AUDIO_MAX_BYTES   (64 * 1024)

const treaai_quick_cmd_t treaai_quick_cmds[TREAAI_QUICK_CMD_COUNT] = {
    { "cmd1", "今日天气",     "查一下今天北京的天气,简洁回答" },
    { "cmd2", "翻译",         "帮我把下一句翻译成英文,只给译文" },
    { "cmd3", "计算",         "帮我计算: " },
    { "cmd4", "讲个短故事",   "讲一个适合大人听的短篇小故事,100字以内" },
    { "cmd5", "设提醒",       "提醒我: " },
    { "cmd6", "闲聊",         "和我聊两句,轻松点" },
};

// ---------------------------------------------------------------------------
// 内部状态
// ---------------------------------------------------------------------------

typedef struct {
    SemaphoreHandle_t lock;
    treaai_state_t state;
    char last_error[64];

    // HTTP
    char host[64];
    int  port;
    char path[128];
    char api_key[128];
    char agent_id[64];

    // 录音累计
    int16_t *audio_buf;
    size_t   audio_len;
    size_t   audio_cap;

    // 回调 (async context)
    QueueHandle_t result_queue;
    TaskHandle_t  worker;
    volatile bool  cancel;
} treaai_ctx_t;

static treaai_ctx_t s_ctx;

// ---------------------------------------------------------------------------
// NVS 工具
// ---------------------------------------------------------------------------

static esp_err_t nvs_open_default(nvs_handle_t *h)
{
    return nvs_open(TREAAI_NVS_NS, NVS_READWRITE, h);
}

esp_err_t treaai_set_api_key(const char *key)
{
    if (!key) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t e = nvs_open_default(&h);
    if (e != ESP_OK) return e;
    e = nvs_set_str(h, "api_key", key);
    if (e == ESP_OK) nvs_commit(h);
    nvs_close(h);
    if (e == ESP_OK) strncpy(s_ctx.api_key, key, sizeof(s_ctx.api_key) - 1);
    return e;
}

esp_err_t treaai_get_api_key(char *out, size_t out_size)
{
    if (!out || out_size == 0) return ESP_ERR_INVALID_ARG;
    // 先查运行缓存
    if (s_ctx.api_key[0]) {
        strncpy(out, s_ctx.api_key, out_size - 1);
        out[out_size - 1] = '\0';
        return ESP_OK;
    }
    nvs_handle_t h;
    esp_err_t e = nvs_open_default(&h);
    if (e != ESP_OK) return e;
    size_t needed = out_size;
    e = nvs_get_str(h, "api_key", out, &needed);
    nvs_close(h);
    return e;
}

esp_err_t treaai_set_endpoint(const char *host, int port, const char *path)
{
    if (!host || !path || port <= 0) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t e = nvs_open_default(&h);
    if (e != ESP_OK) return e;
    nvs_set_str(h, "host", host);
    nvs_set_i32(h, "port", port);
    nvs_set_str(h, "path", path);
    e = nvs_commit(h);
    nvs_close(h);
    if (e == ESP_OK) {
        strncpy(s_ctx.host, host, sizeof(s_ctx.host) - 1);
        s_ctx.port = port;
        strncpy(s_ctx.path, path, sizeof(s_ctx.path) - 1);
    }
    return e;
}

esp_err_t treaai_get_endpoint(char *host, size_t hsize, int *port,
                              char *path, size_t psize)
{
    if (!host || !port || !path) return ESP_ERR_INVALID_ARG;
    if (s_ctx.host[0]) {
        strncpy(host, s_ctx.host, hsize - 1);
        host[hsize - 1] = '\0';
        *port = s_ctx.port;
        strncpy(path, s_ctx.path, psize - 1);
        path[psize - 1] = '\0';
        return ESP_OK;
    }
    nvs_handle_t h;
    esp_err_t e = nvs_open_default(&h);
    if (e != ESP_OK) return e;
    size_t hn = hsize, pn = psize;
    int32_t port32 = 0;
    e = nvs_get_str(h, "host", host, &hn);
    if (e == ESP_OK) e = nvs_get_i32(h, "port", &port32);
    if (e == ESP_OK) e = nvs_get_str(h, "path", path, &pn);
    nvs_close(h);
    if (e != ESP_OK) {
        // 回退到默认
        strncpy(host, TREAAI_DEFAULT_HOST, hsize - 1);
        host[hsize - 1] = '\0';
        *port = TREAAI_DEFAULT_PORT;
        strncpy(path, TREAAI_DEFAULT_PATH, psize - 1);
        path[psize - 1] = '\0';
        return ESP_OK; // 默认值视为 OK
    }
    *port = (int)port32;
    return ESP_OK;
}

esp_err_t treaai_set_quick_prompt(int idx, const char *prompt)
{
    if (idx < 0 || idx >= TREAAI_QUICK_CMD_COUNT || !prompt) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t e = nvs_open_default(&h);
    if (e != ESP_OK) return e;
    char key[16];
    snprintf(key, sizeof(key), "qcmd%d", idx);
    e = nvs_set_str(h, key, prompt);
    if (e == ESP_OK) nvs_commit(h);
    nvs_close(h);
    return e;
}

esp_err_t treaai_get_quick_prompt(int idx, char *out, size_t out_size)
{
    if (idx < 0 || idx >= TREAAI_QUICK_CMD_COUNT || !out || out_size == 0)
        return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t e = nvs_open_default(&h);
    if (e != ESP_OK) {
        // NVS 没初始化, 返回内置默认
        strncpy(out, treaai_quick_cmds[idx].default_prompt, out_size - 1);
        out[out_size - 1] = '\0';
        return ESP_OK;
    }
    char key[16];
    snprintf(key, sizeof(key), "qcmd%d", idx);
    size_t needed = out_size;
    e = nvs_get_str(h, key, out, &needed);
    nvs_close(h);
    if (e != ESP_OK) {
        strncpy(out, treaai_quick_cmds[idx].default_prompt, out_size - 1);
        out[out_size - 1] = '\0';
        return ESP_OK;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// 状态
// ---------------------------------------------------------------------------

static void set_state(treaai_state_t s, const char *err_fmt, ...)
{
    if (xSemaphoreTake(s_ctx.lock, pdMS_TO_TICKS(500)) != pdTRUE) return;
    s_ctx.state = s;
    if (err_fmt) {
        va_list ap;
        va_start(ap, err_fmt);
        vsnprintf(s_ctx.last_error, sizeof(s_ctx.last_error), err_fmt, ap);
        va_end(ap);
    } else {
        s_ctx.last_error[0] = '\0';
    }
    xSemaphoreGive(s_ctx.lock);
}

treaai_state_t treaai_get_state(void)
{
    if (xSemaphoreTake(s_ctx.lock, pdMS_TO_TICKS(100)) != pdTRUE)
        return TREAAI_STATE_ERROR;
    treaai_state_t s = s_ctx.state;
    xSemaphoreGive(s_ctx.lock);
    return s;
}

void treaai_get_status(treaai_status_t *out)
{
    if (!out) return;
    memset(out, 0, sizeof(*out));
    if (xSemaphoreTake(s_ctx.lock, pdMS_TO_TICKS(100)) != pdTRUE) return;
    out->state = s_ctx.state;
    strncpy(out->last_error, s_ctx.last_error, sizeof(out->last_error) - 1);
    xSemaphoreGive(s_ctx.lock);
    // 电量从 BSP 取 (不能 include bsp_battery.h, 让调用者自己填)
    out->battery_soc = -1;
    out->wifi_connected = false;
    out->server_reachable = false;
}

// ---------------------------------------------------------------------------
// 录音累计
// ---------------------------------------------------------------------------

esp_err_t treaai_upload_audio_chunk(const int16_t *pcm, size_t samples)
{
    if (!pcm || samples == 0) return ESP_ERR_INVALID_ARG;
    if (xSemaphoreTake(s_ctx.lock, pdMS_TO_TICKS(500)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    size_t need = (s_ctx.audio_len + samples) * sizeof(int16_t);
    if (need > TREAAI_AUDIO_MAX_BYTES) {
        ESP_LOGW(TAG, "audio buffer full (%zu ≥ %zu), ignoring chunk",
                 need, (size_t)TREAAI_AUDIO_MAX_BYTES);
        xSemaphoreGive(s_ctx.lock);
        return ESP_ERR_NO_MEM;
    }
    if (need > s_ctx.audio_cap) {
        size_t new_cap = s_ctx.audio_cap ? s_ctx.audio_cap * 2 : 4096;
        while (new_cap < need) new_cap *= 2;
        int16_t *nb = realloc(s_ctx.audio_buf, new_cap);
        if (!nb) {
            xSemaphoreGive(s_ctx.lock);
            return ESP_ERR_NO_MEM;
        }
        s_ctx.audio_buf = nb;
        s_ctx.audio_cap = new_cap;
    }
    memcpy(s_ctx.audio_buf + s_ctx.audio_len / 2, pcm, samples * sizeof(int16_t));
    s_ctx.audio_len += samples * sizeof(int16_t);
    xSemaphoreGive(s_ctx.lock);
    set_state(TREAAI_STATE_LISTENING, NULL);
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// HTTP 发送 + worker
// ---------------------------------------------------------------------------

typedef struct {
    char    payload[4096];       // JSON body 或者 base64 chunk (当前设计用 JSON)
    size_t  payload_len;
    bool    is_audio;
    treaai_response_cb_t cb;
    void   *user;
} worker_req_t;

typedef struct {
    char    reply[TREAAI_MAX_RESPONSE_LEN];
    int     audio_len;
    esp_err_t err;
    treaai_response_cb_t cb;
    void   *user;
} worker_result_t;

static esp_err_t do_http_request(const char *json_body, size_t body_len,
                                 char *reply_out, size_t reply_cap)
{
    char url[256];
    snprintf(url, sizeof(url), "https://%s:%d%s",
             s_ctx.host[0] ? s_ctx.host : TREAAI_DEFAULT_HOST,
             s_ctx.port ? s_ctx.port : TREAAI_DEFAULT_PORT,
             s_ctx.path[0] ? s_ctx.path : TREAAI_DEFAULT_PATH);

    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
        .buffer_size = 2048,
        .buffer_size_tx = 2048,
        .cert_pem = NULL, // 用系统默认 (sdkconfig 默认启用 mbedtls CA)
    };
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h) return ESP_ERR_NO_MEM;

    esp_http_client_set_header(h, "Content-Type", "application/json");
    if (s_ctx.api_key[0]) {
        esp_http_client_set_header(h, "Authorization", s_ctx.api_key);
    }

    esp_err_t e = esp_http_client_open(h, (int)body_len);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "http open failed: %s", esp_err_to_name(e));
        esp_http_client_cleanup(h);
        return e;
    }
    int w = esp_http_client_write(h, json_body, (int)body_len);
    if (w < 0 || (size_t)w != body_len) {
        ESP_LOGW(TAG, "http write failed");
        esp_http_client_cleanup(h);
        return ESP_FAIL;
    }

    int code = esp_http_client_fetch_headers(h);
    if (code < 0) {
        ESP_LOGW(TAG, "http fetch failed: %s", esp_err_to_name(code));
        esp_http_client_cleanup(h);
        return ESP_FAIL;
    }

    // 读响应
    int r = esp_http_client_read(h, reply_out, reply_cap - 1);
    esp_http_client_cleanup(h);
    if (r <= 0) {
        ESP_LOGW(TAG, "http read failed or empty, code=%d", code);
        return ESP_FAIL;
    }
    reply_out[r] = '\0';
    return ESP_OK;
}

static void worker_task(void *arg)
{
    worker_req_t *req = (worker_req_t *)arg;
    // 1. base64 编码音频 (如果是 audio 路径)
    char *json = NULL;
    char *b64 = NULL;
    if (req->is_audio) {
        // 在锁内取累计 buffer
        int16_t *pcm = NULL;
        size_t pcm_len = 0;
        if (xSemaphoreTake(s_ctx.lock, pdMS_TO_TICKS(500)) == pdTRUE) {
            pcm = s_ctx.audio_buf;
            pcm_len = s_ctx.audio_len;
            // 清空, 下次录音重新累计
            s_ctx.audio_len = 0;
            xSemaphoreGive(s_ctx.lock);
        }
        if (!pcm || pcm_len == 0) {
            set_state(TREAAI_STATE_ERROR, "no audio captured");
            worker_result_t res = { .err = ESP_ERR_INVALID_STATE,
                                    .cb = req->cb, .user = req->user };
            if (req->cb) req->cb(NULL, 0, req->user);
            free(req);
            vTaskDelete(NULL);
            return;
        }
        // base64
        size_t b64_cap = (pcm_len / 3) * 4 + 8;
        b64 = malloc(b64_cap);
        size_t b64_len = 0;
        mbedtls_base64_encode((unsigned char *)b64, b64_cap, &b64_len,
                              (const unsigned char *)pcm, pcm_len);
        b64[b64_len] = '\0';
        json = treaai_build_audio_request(
            s_ctx.agent_id[0] ? s_ctx.agent_id : "default",
            b64, b64_len);
        free(b64);
    } else {
        json = treaai_build_text_request(
            s_ctx.agent_id[0] ? s_ctx.agent_id : "default",
            req->payload);
    }
    if (!json) {
        set_state(TREAAI_STATE_ERROR, "json build failed");
        if (req->cb) req->cb(NULL, 0, req->user);
        free(req);
        vTaskDelete(NULL);
        return;
    }

    // 2. HTTP
    set_state(TREAAI_STATE_THINKING, NULL);
    char reply[TREAAI_MAX_RESPONSE_LEN];
    esp_err_t e = do_http_request(json, strlen(json),
                                  reply, sizeof(reply));
    free(json);
    if (s_ctx.cancel) {
        ESP_LOGI(TAG, "request cancelled by user");
        set_state(TREAAI_STATE_IDLE, NULL);
        if (req->cb) req->cb(NULL, 0, req->user);
        free(req);
        vTaskDelete(NULL);
        return;
    }
    if (e != ESP_OK) {
        set_state(TREAAI_STATE_ERROR, "http: %s", esp_err_to_name(e));
        if (req->cb) req->cb(NULL, 0, req->user);
        free(req);
        vTaskDelete(NULL);
        return;
    }

    // 3. 解析 JSON
    char text_out[TREAAI_MAX_RESPONSE_LEN];
    char audio_out[4096];
    size_t audio_len = 0;
    e = treaai_parse_response(reply, text_out, sizeof(text_out),
                              audio_out, sizeof(audio_out), &audio_len);
    if (e == ESP_OK) {
        set_state(TREAAI_STATE_IDLE, NULL);
    } else {
        set_state(TREAAI_STATE_ERROR, "parse: %s", esp_err_to_name(e));
    }
    if (req->cb) req->cb(e == ESP_OK ? text_out : NULL,
                          (int)audio_len, req->user);
    free(req);
    vTaskDelete(NULL);
}

esp_err_t treaai_send_text(const char *text, treaai_response_cb_t cb, void *user)
{
    if (!text || !*text) return ESP_ERR_INVALID_ARG;
    if (treaai_get_state() != TREAAI_STATE_IDLE) return ESP_ERR_INVALID_STATE;

    worker_req_t *req = calloc(1, sizeof(*req));
    if (!req) return ESP_ERR_NO_MEM;
    strncpy(req->payload, text, sizeof(req->payload) - 1);
    req->payload_len = strlen(req->payload);
    req->is_audio = false;
    req->cb = cb;
    req->user = user;

    s_ctx.cancel = false;
    set_state(TREAAI_STATE_CONNECTING, NULL);

    BaseType_t ok = xTaskCreate(worker_task, "treaai_http", 8192, req, 4, &s_ctx.worker);
    if (ok != pdPASS) {
        free(req);
        set_state(TREAAI_STATE_ERROR, "no ram for task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t treaai_send_audio(treaai_response_cb_t cb, void *user)
{
    if (treaai_get_state() == TREAAI_STATE_ERROR) return ESP_ERR_INVALID_STATE;
    if (xSemaphoreTake(s_ctx.lock, pdMS_TO_TICKS(500)) != pdTRUE)
        return ESP_ERR_TIMEOUT;
    size_t has = s_ctx.audio_len;
    xSemaphoreGive(s_ctx.lock);
    if (has == 0) return ESP_ERR_INVALID_STATE;

    worker_req_t *req = calloc(1, sizeof(*req));
    if (!req) return ESP_ERR_NO_MEM;
    req->is_audio = true;
    req->cb = cb;
    req->user = user;

    s_ctx.cancel = false;
    set_state(TREAAI_STATE_CONNECTING, NULL);

    BaseType_t ok = xTaskCreate(worker_task, "treaai_http", 8192, req, 4, &s_ctx.worker);
    if (ok != pdPASS) {
        free(req);
        set_state(TREAAI_STATE_ERROR, "no ram for task");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void treaai_cancel(void)
{
    s_ctx.cancel = true;
    set_state(TREAAI_STATE_IDLE, NULL);
}

// ---------------------------------------------------------------------------
// 连通性测试
// ---------------------------------------------------------------------------

esp_err_t treaai_test_connectivity(bool *out_wifi_ok, bool *out_server_ok)
{
    if (out_wifi_ok) *out_wifi_ok = false;
    if (out_server_ok) *out_server_ok = false;
    // 简单实现: 尝试 HTTP 空请求, 能 reach 就算 server_ok
    char json[] = "{\"agent_id\":\"default\",\"message\":\"ping\"}";
    char reply[64];
    esp_err_t e = do_http_request(json, strlen(json), reply, sizeof(reply));
    if (out_server_ok) *out_server_ok = (e == ESP_OK);
    if (out_wifi_ok)   *out_wifi_ok   = true; // 能发 HTTP 就有网
    return e;
}

// ---------------------------------------------------------------------------
// 初始化
// ---------------------------------------------------------------------------

esp_err_t treaai_init(void)
{
    memset(&s_ctx, 0, sizeof(s_ctx));
    s_ctx.lock = xSemaphoreCreateMutex();
    if (!s_ctx.lock) return ESP_ERR_NO_MEM;

    // 默认端点
    strncpy(s_ctx.host, TREAAI_DEFAULT_HOST, sizeof(s_ctx.host) - 1);
    s_ctx.port = TREAAI_DEFAULT_PORT;
    strncpy(s_ctx.path, TREAAI_DEFAULT_PATH, sizeof(s_ctx.path) - 1);
    strncpy(s_ctx.agent_id, "treaai-passport", sizeof(s_ctx.agent_id) - 1);

    // 尝试从 NVS 读 (可能 NVS 还没 init, 失败就保持默认)
    nvs_flash_init(); // 幂等
    char h[64], p[128], k[128];
    int port;
    if (treaai_get_endpoint(h, sizeof(h), &port, p, sizeof(p)) == ESP_OK) {
        treaai_set_endpoint(h, port, p); // 回填到 cache
    }
    if (treaai_get_api_key(k, sizeof(k)) == ESP_OK) {
        treaai_set_api_key(k);
    }

    s_ctx.audio_buf = NULL;
    s_ctx.audio_len = 0;
    s_ctx.audio_cap = 0;
    s_ctx.state = TREAAI_STATE_IDLE;
    return ESP_OK;
}
