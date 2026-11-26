// main/kl8_result.c —— 拉取并解析快乐8开奖结果。
// 数据源:中国福利彩票官网开奖公告接口(HTTPS,证书用 IDF 内置 bundle 校验)。
// 与 ssq_result.c 同构,区别是每期 20 个号(1-80),没有蓝球。
#include "kl8_result.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "net_wifi.h"

static const char *TAG = "kl8_result";

#define KL8_API_URL \
    "https://www.cwl.gov.cn/cwl_admin/front/cwlkj/search/kjxx/findDrawNotice?name=kl8&issueCount=" \
    "10"
#define KL8_REFERER "https://www.cwl.gov.cn/ygkj/wqkjgg/kl8/"
#define KL8_BODY_MAX (40 * 1024)   // 实测 10 期约 25 KB
#define KL8_WAIT_WIFI_SECONDS 20

static kl8_issue_t s_issues[KL8_ISSUE_MAX];
static volatile int s_count;
static volatile kl8_state_t s_state = KL8_STATE_IDLE;
static SemaphoreHandle_t s_request_sem;
static void (*s_on_update)(void);

static void notify_update(void) {
    if (s_on_update) s_on_update();
}

// "2026-11-24(二)" 只保留前 10 个字符。
static void copy_date(char *dst, size_t cap, const char *src) {
    size_t n = src ? strlen(src) : 0;
    if (n > 10) n = 10;
    if (n >= cap) n = cap - 1;
    if (n) memcpy(dst, src, n);
    dst[n] = '\0';
}

// "09,12,...":按逗号切分,必须正好解析出 n 个 1..80 的号。
static bool parse_nums(uint8_t *dst, const char *src, int n) {
    if (!src) return false;
    const char *p = src;
    for (int i = 0; i < n; i++) {
        char *end = NULL;
        long v = strtol(p, &end, 10);
        if (end == p || v < 1 || v > 80) return false;
        dst[i] = (uint8_t)v;
        p = end;
        if (i + 1 < n) {
            while (*p == ' ') p++;
            if (*p != ',') return false;
            p++;
        }
    }
    while (*p == ' ') p++;
    return *p == '\0';
}

// 从 "key":" 后面读一个字符串值(到下一个双引号为止)。
static bool read_value(const char *p, char *out, size_t cap) {
    const char *q = p;
    while (*q && *q != '"') q++;
    if (*q != '"') return false;
    size_t n = (size_t)(q - p);
    if (n + 1 > cap) return false;
    memcpy(out, p, n);
    out[n] = '\0';
    return true;
}

// 只取 code / date / red 三个字段,不建 cJSON 树。
// 25 KB 响应在 C3 上构树要 40 KB 以上堆,和 LVGL 抢内存会直接失败。
static bool parse_body(const char *body, size_t len) {
    static const char KEY_CODE[] = "\"code\":\"";
    static const char KEY_DATE[] = "\"date\":\"";
    static const char KEY_RED[] = "\"red\":\"";

    kl8_issue_t parsed[KL8_ISSUE_MAX];
    int n = 0;
    const char *cur = body;
    const char *end = body + len;

    while (n < KL8_ISSUE_MAX) {
        const char *code = strstr(cur, KEY_CODE);
        if (!code || code >= end) break;
        code += sizeof(KEY_CODE) - 1;
        const char *date = strstr(code, KEY_DATE);
        const char *red = strstr(code, KEY_RED);
        if (!date || !red || red >= end) break;

        kl8_issue_t *it = &parsed[n];
        memset(it, 0, sizeof(*it));
        char code_buf[16];
        char date_buf[16];
        char red_buf[96];
        if (!read_value(code, code_buf, sizeof(code_buf))) break;
        if (!read_value(date + sizeof(KEY_DATE) - 1, date_buf, sizeof(date_buf))) break;
        const char *red_val = red + sizeof(KEY_RED) - 1;
        if (!read_value(red_val, red_buf, sizeof(red_buf))) break;

        snprintf(it->code, sizeof(it->code), "%s", code_buf);
        copy_date(it->date, sizeof(it->date), date_buf);
        if (!parse_nums(it->nums, red_buf, KL8_NUM_COUNT)) {
            ESP_LOGW(TAG, "期号 %s 的开奖号解析失败,跳过", code_buf);
        } else {
            n++;
        }

        // 跳过本期的 red 值,继续找下一期。
        const char *red_end = red_val;
        while (*red_end && *red_end != '"') red_end++;
        cur = (*red_end == '"') ? red_end + 1 : end;
    }

    if (n == 0) {
        ESP_LOGE(TAG, "没有可用的开奖记录");
        return false;
    }
    memcpy(s_issues, parsed, sizeof(kl8_issue_t) * (size_t)n);
    s_count = n;
    ESP_LOGI(TAG, "已缓存 %d 期,最新 %s(%s) 首尾 %d..%d", n, s_issues[0].code, s_issues[0].date,
             s_issues[0].nums[0], s_issues[0].nums[KL8_NUM_COUNT - 1]);
    return true;
}

static bool http_fetch(void) {
    char *body = heap_caps_malloc(KL8_BODY_MAX, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!body) body = malloc(KL8_BODY_MAX);
    if (!body) {
        ESP_LOGE(TAG, "缓冲区分配失败");
        return false;
    }

    const esp_http_client_config_t cfg = {
        .url = KL8_API_URL,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = 15000,
        .buffer_size = 2048,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) {
        free(body);
        ESP_LOGE(TAG, "HTTP 客户端创建失败");
        return false;
    }
    esp_http_client_set_header(client, "Referer", KL8_REFERER);
    esp_http_client_set_header(client, "User-Agent", "Mozilla/5.0");

    bool ok = false;
    if (esp_http_client_open(client, 0) != ESP_OK) {
        ESP_LOGE(TAG, "连接开奖接口失败");
        goto cleanup;
    }
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    if (status != 200) {
        ESP_LOGE(TAG, "HTTP 状态码 %d", status);
        goto cleanup;
    }
    size_t total = 0;
    while (total < KL8_BODY_MAX - 1) {
        int read = esp_http_client_read(client, body + total, (int)(KL8_BODY_MAX - 1 - total));
        if (read <= 0) break;
        total += (size_t)read;
    }
    body[total] = '\0';
    ESP_LOGI(TAG, "收到 %u 字节", (unsigned)total);
    ok = total > 0 && parse_body(body, total);

cleanup:
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(body);
    return ok;
}

static void fetch_task(void *arg) {
    (void)arg;
    for (;;) {
        xSemaphoreTake(s_request_sem, portMAX_DELAY);

        s_state = KL8_STATE_LOADING;
        notify_update();

        // 凭据是 net_wifi_start() 才从 NVS 读进来的,不能拿「此刻有没有配置」直接判死;
        // 先等联网,等不到再看配置状态(第 2 秒起就会读好了,没凭据不必干等 20 秒)。
        for (int i = 0; i < KL8_WAIT_WIFI_SECONDS && !net_wifi_connected(); i++) {
            if (i > 0 && !net_wifi_configured()) break;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        if (!net_wifi_connected()) {
            ESP_LOGW(TAG, "网络未就绪,放弃本次拉取");
            s_state = KL8_STATE_NO_WIFI;
            notify_update();
            continue;
        }

        // 和双色球同时拉取时,TLS 握手可能挤爆内存导致 open 失败;
        // 隔 2 秒重试,等另一路拉完再试,比直接报「拉取失败」稳。
        bool ok = false;
        for (int attempt = 0; attempt < 3 && !ok; attempt++) {
            if (attempt > 0) {
                ESP_LOGW(TAG, "第 %d 次重试", attempt);
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
            ok = http_fetch();
        }
        s_state = ok ? KL8_STATE_OK : KL8_STATE_FAIL;
        notify_update();
    }
}

void kl8_result_init(void (*on_update)(void)) {
    s_on_update = on_update;
    if (s_request_sem) return;
    s_request_sem = xSemaphoreCreateBinary();
    if (!s_request_sem) {
        ESP_LOGE(TAG, "信号量创建失败,快乐8开奖不可用");
        return;
    }
    // 栈比双色球略大:响应里奖级明细多,解析递归更深。
    if (xTaskCreate(fetch_task, "kl8_fetch", 8192, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "拉取任务创建失败");
        vSemaphoreDelete(s_request_sem);
        s_request_sem = NULL;
    }
}

void kl8_result_request(void) {
    if (!s_request_sem) return;
    xSemaphoreGive(s_request_sem);
}

kl8_state_t kl8_result_state(void) { return s_state; }

int kl8_result_count(void) { return s_count; }

const kl8_issue_t *kl8_result_at(int index) {
    if (index < 0 || index >= s_count) return NULL;
    return &s_issues[index];
}

bool kl8_issue_has(const kl8_issue_t *issue, uint8_t num) {
    if (!issue) return false;
    for (int i = 0; i < KL8_NUM_COUNT; i++) {
        if (issue->nums[i] == num) return true;
    }
    return false;
}
