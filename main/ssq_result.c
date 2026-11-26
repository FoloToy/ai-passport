// main/ssq_result.c —— 拉取并解析双色球开奖结果。
// 数据源:中国福利彩票官网开奖公告接口(HTTPS,证书用 IDF 内置 bundle 校验)。
// 解析只取用得到的字段(期号/日期/红球/蓝球),其余奖级明细忽略。
#include "ssq_result.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "net_wifi.h"

static const char *TAG = "ssq_result";

#define SSQ_API_URL \
    "https://www.cwl.gov.cn/cwl_admin/front/cwlkj/search/kjxx/findDrawNotice?name=ssq&issueCount=" \
    "10"
#define SSQ_REFERER "https://www.cwl.gov.cn/ygkj/wqkjgg/ssq/"
// 10 期实测约 9.5 KB;16 KB 够用,峰值越省越不容易和 TLS 抢爆堆。
#define SSQ_BODY_MAX (16 * 1024)
#define SSQ_WAIT_WIFI_SECONDS 20

static ssq_issue_t s_issues[SSQ_ISSUE_MAX];
static volatile int s_count;
static volatile ssq_state_t s_state = SSQ_STATE_IDLE;
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

// "02,10,16,22,26,30" -> red[6]
static bool parse_red(uint8_t *dst, const char *src) {
    int a = 0, b = 0, c = 0, d = 0, e = 0, f = 0;
    if (!src) return false;
    if (sscanf(src, "%d,%d,%d,%d,%d,%d", &a, &b, &c, &d, &e, &f) != 6) return false;
    dst[0] = (uint8_t)a;
    dst[1] = (uint8_t)b;
    dst[2] = (uint8_t)c;
    dst[3] = (uint8_t)d;
    dst[4] = (uint8_t)e;
    dst[5] = (uint8_t)f;
    return true;
}

// 从 "key":" 之后读一个字符串值(读到下一个双引号)。
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

// 只扫 code / date / red / blue 四个字段,不建 cJSON 树。
// 9.5 KB 响应建树要 20~30 KB 堆,和 TLS 会话叠在一起直接 OOM;
// 之前失败就是这条(报的是「JSON 解析失败」,实际是 cJSON_Parse 返回 NULL)。
static bool parse_body(const char *body, size_t len) {
    static const char KEY_CODE[] = "\"code\":\"";
    static const char KEY_DATE[] = "\"date\":\"";
    static const char KEY_RED[] = "\"red\":\"";
    static const char KEY_BLUE[] = "\"blue\":\"";

    ssq_issue_t parsed[SSQ_ISSUE_MAX];
    int n = 0;
    const char *cur = body;
    const char *end = body + len;

    while (n < SSQ_ISSUE_MAX) {
        const char *code = strstr(cur, KEY_CODE);
        if (!code || code >= end) break;
        const char *date = strstr(code, KEY_DATE);
        const char *red = strstr(code, KEY_RED);
        const char *blue = strstr(code, KEY_BLUE);
        if (!date || !red || !blue || date >= end || red >= end || blue >= end) break;

        char code_buf[12];
        char date_buf[20];
        char red_buf[32];
        char blue_buf[8];
        if (!read_value(code + sizeof(KEY_CODE) - 1, code_buf, sizeof(code_buf))) break;
        if (!read_value(date + sizeof(KEY_DATE) - 1, date_buf, sizeof(date_buf))) break;
        if (!read_value(red + sizeof(KEY_RED) - 1, red_buf, sizeof(red_buf))) break;
        if (!read_value(blue + sizeof(KEY_BLUE) - 1, blue_buf, sizeof(blue_buf))) break;

        ssq_issue_t *it = &parsed[n];
        memset(it, 0, sizeof(*it));
        snprintf(it->code, sizeof(it->code), "%s", code_buf);
        copy_date(it->date, sizeof(it->date), date_buf);
        if (parse_red(it->red, red_buf)) {
            it->blue = (uint8_t)atoi(blue_buf);
            n++;
        }
        // 本期读完,从 blue 之后继续找下一期(字段顺序固定:code→date→red→blue)。
        cur = blue;
    }

    if (n == 0) {
        ESP_LOGE(TAG, "没有可用的开奖记录");
        return false;
    }
    memcpy(s_issues, parsed, sizeof(ssq_issue_t) * (size_t)n);
    s_count = n;
    ESP_LOGI(TAG, "已缓存 %d 期,最新 %s(%s) 红 %d %d %d %d %d %d 蓝 %d", n, s_issues[0].code,
             s_issues[0].date, s_issues[0].red[0], s_issues[0].red[1], s_issues[0].red[2],
             s_issues[0].red[3], s_issues[0].red[4], s_issues[0].red[5], s_issues[0].blue);
    return true;
}

static bool http_fetch(void) {
    // ESP32-C3 没有 PSRAM,直接用内部堆;失败就是真没内存了。
    char *body = malloc(SSQ_BODY_MAX);
    if (!body) {
        ESP_LOGE(TAG, "缓冲区分配失败");
        return false;
    }

    const esp_http_client_config_t cfg = {
        .url = SSQ_API_URL,
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
    esp_http_client_set_header(client, "Referer", SSQ_REFERER);
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
    while (total < SSQ_BODY_MAX - 1) {
        int read = esp_http_client_read(client, body + total, (int)(SSQ_BODY_MAX - 1 - total));
        if (read <= 0) break;
        total += (size_t)read;
    }
    body[total] = '\0';
    // 可观测点:这个项目两次栽在解析时堆不够,出问题先看这行剩余多少。
    ESP_LOGI(TAG, "收到 %u 字节,剩余堆 %u", (unsigned)total,
             (unsigned)esp_get_free_heap_size());
    if (total == SSQ_BODY_MAX - 1) ESP_LOGW(TAG, "响应被截断(缓冲 %u 字节)", (unsigned)SSQ_BODY_MAX);
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

        s_state = SSQ_STATE_LOADING;
        notify_update();

        // 开机后网络可能还没起来:最多等 20 秒。
        // 凭据是 net_wifi_start() 才从 NVS 读进来的,不能拿「此刻有没有配置」直接判死;
        // 先等联网,等不到再看配置状态(第 2 秒起就会读好了,没凭据不必干等 20 秒)。
        for (int i = 0; i < SSQ_WAIT_WIFI_SECONDS && !net_wifi_connected(); i++) {
            if (i > 0 && !net_wifi_configured()) break;
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        if (!net_wifi_connected()) {
            ESP_LOGW(TAG, "网络未就绪,放弃本次拉取");
            s_state = SSQ_STATE_NO_WIFI;
            notify_update();
            continue;
        }

        s_state = http_fetch() ? SSQ_STATE_OK : SSQ_STATE_FAIL;
        notify_update();
    }
}

void ssq_result_init(void (*on_update)(void)) {
    s_on_update = on_update;
    if (s_request_sem) return;
    s_request_sem = xSemaphoreCreateBinary();
    if (!s_request_sem) {
        ESP_LOGE(TAG, "信号量创建失败,开奖结果不可用");
        return;
    }
    if (xTaskCreate(fetch_task, "ssq_fetch", 8192, NULL, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "拉取任务创建失败");
        vSemaphoreDelete(s_request_sem);
        s_request_sem = NULL;
    }
}

void ssq_result_request(void) {
    if (!s_request_sem) return;
    // 二进制信号量天然合并重复请求:已有待处理请求时这次 give 直接返回失败,无副作用。
    xSemaphoreGive(s_request_sem);
}

ssq_state_t ssq_result_state(void) {
    return s_state;
}

int ssq_result_count(void) {
    return s_count;
}

const ssq_issue_t *ssq_result_at(int index) {
    if (index < 0 || index >= s_count) return NULL;
    return &s_issues[index];
}
