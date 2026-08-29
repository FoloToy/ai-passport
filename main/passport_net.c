// main/passport_net.c —— 网络任务实现。
//
// 突发式工作流（实现文档 7.1/7.5 节）：
//   请求 → 扫描 → 连已配置 SSID → SNTP 校时（如需要）→ (重)auth
//        → 业务调用（me/history/check-in/heartbeat）→ 更新 NVS 缓存
//        → esp_wifi_stop() → 投递 UI 事件
// 心跳门控：仅当当前关联 SSID == lab_ssid（默认 SparkMinds_IoT）才发心跳，
// 这是正确性要求（在家同步不得被记为在馆）。
#include "passport_net.h"

#include "passport_core.h"
#include "passport_store.h"
#include "passport_ui.h"

#include "esp_crt_bundle.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sntp.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_wifi_default.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "jsmn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static const char *TAG = "smpass_net";

// ---------------------------------------------------------------------------
// 配置（API 基址编译期可配，实现文档 6.1 节）
// ---------------------------------------------------------------------------
#ifndef CONFIG_PASSPORT_API_BASE
#define CONFIG_PASSPORT_API_BASE "https://api.sparkminds.cn/api"
#endif

#define WIFI_CONNECT_TIMEOUT_MS  10000
#define WIFI_SCAN_TIMEOUT_MS     4000
#define SNTP_WAIT_TIMEOUT_MS     8000
#define HTTP_TIMEOUT_MS          10000
#define HTTP_BUF_SIZE            4096
#define NET_TASK_STACK           8192
#define NET_REQ_QUEUE_LEN        4

#define BIT_GOT_IP      (1 << 0)
#define BIT_CONNECT_FAIL (1 << 1)
#define BIT_SCAN_DONE   (1 << 2)

// ---------------------------------------------------------------------------
// 模块状态
// ---------------------------------------------------------------------------
static QueueHandle_t   s_ui_queue;
static QueueHandle_t   s_req_queue;
static TaskHandle_t    s_task;
static EventGroupHandle_t s_wifi_events;
static esp_netif_t    *s_sta_netif;
static esp_event_handler_instance_t s_wifi_handler;
static esp_event_handler_instance_t s_ip_handler;
static bool            s_wifi_initialized;
static bool            s_wifi_started;
static volatile bool   s_busy;

static SemaphoreHandle_t s_history_lock;
static passport_point_item_t s_history[PASSPORT_HISTORY_MAX];
static int                 s_history_count;

// ---------------------------------------------------------------------------
// UI 事件投递（网络任务只允许做这一件事与 UI 通信）
// ---------------------------------------------------------------------------
static void post_ui(passport_ui_evt_type_t type, int32_t arg1)
{
    if (!s_ui_queue) return;
    passport_ui_evt_t evt = { .type = type, .arg1 = arg1 };
    xQueueSend(s_ui_queue, &evt, 0);
}

bool passport_net_is_busy(void)
{
    return s_busy;
}

int passport_net_history(passport_point_item_t out[PASSPORT_HISTORY_MAX])
{
    int n = 0;
    if (xSemaphoreTake(s_history_lock, pdMS_TO_TICKS(100)) == pdTRUE) {
        n = s_history_count;
        if (n > 0) memcpy(out, s_history, sizeof(passport_point_item_t) * n);
        xSemaphoreGive(s_history_lock);
    }
    return n;
}

// ---------------------------------------------------------------------------
// Wi-Fi 突发连接
// ---------------------------------------------------------------------------
static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base;
    if (id == WIFI_EVENT_SCAN_DONE) {
        xEventGroupSetBits(s_wifi_events, BIT_SCAN_DONE);
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_events, BIT_CONNECT_FAIL);
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)base; (void)id; (void)data;
    xEventGroupSetBits(s_wifi_events, BIT_GOT_IP);
}

static esp_err_t wifi_stack_start(void)
{
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;

    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) return ESP_ERR_NO_MEM;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) return err;
    s_wifi_initialized = true;

    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                              on_wifi_event, NULL, &s_wifi_handler);
    if (err != ESP_OK) return err;
    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                              on_ip_event, NULL, &s_ip_handler);
    if (err != ESP_OK) return err;

    err = esp_wifi_set_storage(WIFI_STORAGE_RAM);
    if (err != ESP_OK) return err;
    err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) return err;
    err = esp_wifi_start();
    if (err != ESP_OK) return err;
    s_wifi_started = true;
    return ESP_OK;
}

static void wifi_stack_stop(void)
{
    if (s_wifi_started) {
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_wifi_started = false;
    }
    if (s_wifi_handler) {
        esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, s_wifi_handler);
        s_wifi_handler = NULL;
    }
    if (s_ip_handler) {
        esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, s_ip_handler);
        s_ip_handler = NULL;
    }
    if (s_wifi_initialized) {
        esp_wifi_deinit();
        s_wifi_initialized = false;
    }
    if (s_sta_netif) {
        esp_netif_destroy_default_wifi(s_sta_netif);
        s_sta_netif = NULL;
    }
}

// 读取两组已配置 Wi-Fi；返回组数
static int load_wifi_configs(wifi_config_t cfgs[2])
{
    int n = 0;
    const char *ssid_keys[2] = { PASSPORT_KEY_WIFI1_SSID, PASSPORT_KEY_WIFI2_SSID };
    const char *pass_keys[2] = { PASSPORT_KEY_WIFI1_PASS, PASSPORT_KEY_WIFI2_PASS };
    for (int i = 0; i < 2; i++) {
        char ssid[33] = { 0 };
        char pass[65] = { 0 };
        if (passport_store_get_string(ssid_keys[i], ssid, sizeof(ssid)) != ESP_OK ||
            ssid[0] == '\0') {
            continue;
        }
        passport_store_get_string(pass_keys[i], pass, sizeof(pass));
        memset(&cfgs[n], 0, sizeof(wifi_config_t));
        strlcpy((char *)cfgs[n].sta.ssid, ssid, sizeof(cfgs[n].sta.ssid));
        strlcpy((char *)cfgs[n].sta.password, pass, sizeof(cfgs[n].sta.password));
        n++;
    }
    return n;
}

// 扫描一次，返回命中的已配置 SSID 下标（0/1），未命中返回 -1
static int scan_for_configured(wifi_config_t cfgs[2], int count)
{
    xEventGroupClearBits(s_wifi_events, BIT_SCAN_DONE);
    esp_err_t err = esp_wifi_scan_start(NULL, false);
    if (err != ESP_OK) return -1;
    EventBits_t bits = xEventGroupWaitBits(s_wifi_events, BIT_SCAN_DONE,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_SCAN_TIMEOUT_MS));
    if (!(bits & BIT_SCAN_DONE)) return -1;

    uint16_t total = 0;
    esp_wifi_scan_get_ap_num(&total);
    if (total == 0) return -1;
    if (total > 16) total = 16;
    wifi_ap_record_t records[16];
    if (esp_wifi_scan_get_ap_records(&total, records) != ESP_OK) return -1;

    for (int i = 0; i < count; i++) {
        for (int j = 0; j < total; j++) {
            if (strcmp((const char *)records[j].ssid,
                       (const char *)cfgs[i].sta.ssid) == 0) {
                return i;
            }
        }
    }
    return -1;
}

// 连接指定配置并等待拿到 IP。成功返回 ESP_OK。
static esp_err_t wifi_connect_one(const wifi_config_t *cfg)
{
    xEventGroupClearBits(s_wifi_events, BIT_GOT_IP | BIT_CONNECT_FAIL);
    esp_err_t err = esp_wifi_set_config(WIFI_IF_STA, (wifi_config_t *)cfg);
    if (err != ESP_OK) return err;
    err = esp_wifi_connect();
    if (err != ESP_OK) return err;

    EventBits_t bits = xEventGroupWaitBits(s_wifi_events,
                                           BIT_GOT_IP | BIT_CONNECT_FAIL,
                                           pdTRUE, pdFALSE,
                                           pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
    return (bits & BIT_GOT_IP) ? ESP_OK : ESP_FAIL;
}

// 突发连接：扫描优先（心跳场景可判馆外），命中哪个连哪个。
// 成功时把实际关联的 SSID 写入 connected_ssid（可空）。
static esp_err_t wifi_burst_connect(char *connected_ssid, size_t ssid_len)
{
    esp_err_t err = wifi_stack_start();
    if (err != ESP_OK) return err;

    wifi_config_t cfgs[2];
    int count = load_wifi_configs(cfgs);
    if (count == 0) {
        ESP_LOGW(TAG, "未配置任何 Wi-Fi");
        return ESP_ERR_NOT_FOUND;
    }

    int picked = scan_for_configured(cfgs, count);
    if (picked < 0) {
        ESP_LOGI(TAG, "扫描未命中已配置 SSID（馆外/网络不可见）");
        return ESP_ERR_NOT_FOUND;
    }

    err = wifi_connect_one(&cfgs[picked]);
    if (err != ESP_OK && count > 1 && picked == 0) {
        err = wifi_connect_one(&cfgs[1]);   // 校区连不上时试家庭网
    }
    if (err == ESP_OK && connected_ssid) {
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            strlcpy(connected_ssid, (const char *)ap.ssid, ssid_len);
        }
    }
    return err;
}

// ---------------------------------------------------------------------------
// SNTP 校时（实现文档 7.4 节）
// ---------------------------------------------------------------------------
static bool time_is_valid_now(void)
{
    time_t now = time(NULL);
    return passport_time_is_valid((uint32_t)now);
}

static esp_err_t sntp_sync_if_needed(void)
{
    if (time_is_valid_now()) {
        passport_store_set_u8(PASSPORT_KEY_TIME_VALID, 1);
        return ESP_OK;
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    if (!esp_sntp_enabled()) esp_sntp_init();

    int64_t deadline = esp_timer_get_time() + (int64_t)SNTP_WAIT_TIMEOUT_MS * 1000;
    while (esp_timer_get_time() < deadline) {
        if (time_is_valid_now()) {
            setenv("TZ", "CST-8", 1);   // Asia/Shanghai（无 DST）
            tzset();
            esp_sntp_stop();
            passport_store_set_u8(PASSPORT_KEY_TIME_VALID, 1);
            post_ui(PASSPORT_UI_EVT_TIME_VALID, 0);
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    esp_sntp_stop();
    return ESP_ERR_TIMEOUT;
}

// ---------------------------------------------------------------------------
// HTTPS 客户端（TLS 证书校验全程开启，禁止跳过校验——实现文档 5.3/10 节）
// ---------------------------------------------------------------------------
typedef struct {
    char  *buf;
    size_t len;
} http_sink_t;

static esp_err_t http_on_event(esp_http_client_event_t *e)
{
    http_sink_t *sink = (http_sink_t *)e->user_data;
    if (e->event_id == HTTP_EVENT_ON_DATA && sink && sink->buf) {
        size_t room = HTTP_BUF_SIZE - 1 - sink->len;
        size_t take = e->data_len < room ? e->data_len : room;
        memcpy(sink->buf + sink->len, e->data, take);
        sink->len += take;
        sink->buf[sink->len] = '\0';
    }
    return ESP_OK;
}

// method: 用 esp_http_client_set_method 设置；body 可空。
// 成功（2xx）返回 ESP_OK 且 sink.buf 为响应体；401 返回 ESP_ERR_INVALID_STATE
// 以便调用方重走 auth；其他返回 ESP_FAIL。
static esp_err_t http_json(esp_http_client_method_t method, const char *path,
                           const char *bearer, const char *body,
                           http_sink_t *sink)
{
    char url[160];
    snprintf(url, sizeof(url), "%s%s", CONFIG_PASSPORT_API_BASE, path);

    sink->len = 0;
    if (sink->buf) sink->buf[0] = '\0';

    esp_http_client_config_t cfg = {
        .url = url,
        .method = method,
        .event_handler = http_on_event,
        .user_data = sink,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .timeout_ms = HTTP_TIMEOUT_MS,
        .disable_auto_redirect = true,
    };
    esp_http_client_handle_t client = esp_http_client_init(&cfg);
    if (!client) return ESP_ERR_NO_MEM;

    esp_http_client_set_header(client, "Content-Type", "application/json");
    if (bearer && bearer[0]) {
        char auth[96];
        snprintf(auth, sizeof(auth), "Bearer %s", bearer);
        esp_http_client_set_header(client, "Authorization", auth);
    }
    if (body) esp_http_client_set_post_field(client, body, (int)strlen(body));

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) return err;
    if (status == 401) return ESP_ERR_INVALID_STATE;
    if (status < 200 || status >= 300) {
        ESP_LOGW(TAG, "HTTP %s %s -> %d", method == HTTP_METHOD_GET ? "GET" : "POST",
                 path, status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// JSON 提取（jsmn；响应体都很小）
// ---------------------------------------------------------------------------
static bool json_eq(const char *json, const jsmntok_t *tok, const char *s)
{
    return tok->type == JSMN_STRING &&
           (int)strlen(s) == tok->end - tok->start &&
           strncmp(json + tok->start, s, (size_t)(tok->end - tok->start)) == 0;
}

// token 子树总长度（含自身；jsmn 不带父指针，递归计算）
static int tok_skip(const jsmntok_t *t, int idx)
{
    if (t[idx].type == JSMN_OBJECT) {
        int i = idx + 1;
        for (int p = 0; p < t[idx].size; p++) {
            i += tok_skip(t, i);   // key
            i += tok_skip(t, i);   // value
        }
        return i - idx;
    }
    if (t[idx].type == JSMN_ARRAY) {
        int i = idx + 1;
        for (int k = 0; k < t[idx].size; k++) i += tok_skip(t, i);
        return i - idx;
    }
    return 1;
}

// 在 object 的顶层找 key，返回值 token 下标；未找到返回 -1
static int json_find(const char *json, const jsmntok_t *toks, int count,
                     int obj_idx, const char *key)
{
    if (obj_idx < 0 || toks[obj_idx].type != JSMN_OBJECT) return -1;
    int idx = obj_idx + 1;
    int pairs = toks[obj_idx].size;
    for (int p = 0; p < pairs && idx < count; p++) {
        if (json_eq(json, &toks[idx], key)) return idx + 1;
        idx += tok_skip(toks, idx);          // key（string 原子）
        if (idx >= count) break;
        idx += tok_skip(toks, idx);          // value（可能是整个子树）
    }
    return -1;
}

static void json_str(const char *json, const jsmntok_t *tok, char *out, size_t len)
{
    int n = tok->end - tok->start;
    if (n < 0) n = 0;
    if ((size_t)n >= len) n = (int)len - 1;
    memcpy(out, json + tok->start, (size_t)n);
    out[n] = '\0';
}

static int32_t json_int(const char *json, const jsmntok_t *tok)
{
    char tmp[16];
    json_str(json, tok, tmp, sizeof(tmp));
    return (int32_t)strtol(tmp, NULL, 10);
}

#define JSMN_TOKENS 96

static int json_parse(const char *json, jsmntok_t *toks, int cap)
{
    jsmn_parser parser;
    jsmn_init(&parser);
    return jsmn_parse(&parser, json, strlen(json), toks, (unsigned int)cap);
}

// ---------------------------------------------------------------------------
// 业务调用
// ---------------------------------------------------------------------------
static char *s_http_buf;   // 突发期间分配

// auth 签名换 token（实现文档 6.3 节）。成功写 NVS pp_token 与学员缓存。
static esp_err_t do_auth(void)
{
    char pid[32];
    uint8_t secret[PASSPORT_SECRET_LEN];
    esp_err_t err = passport_store_get_string(PASSPORT_KEY_PID, pid, sizeof(pid));
    if (err != ESP_OK) goto out;
    err = passport_store_get_secret(secret);
    if (err != ESP_OK) goto out;

    uint32_t ts = (uint32_t)time(NULL);
    char sig[PASSPORT_SIG_HEX_LEN + 1];
    passport_auth_sig(secret, pid, ts, sig);

    char body[160];
    snprintf(body, sizeof(body),
             "{\"passport_id\":\"%s\",\"ts\":%lu,\"sig\":\"%s\"}",
             pid, (unsigned long)ts, sig);
    memset(secret, 0, sizeof(secret));
    // sig 属敏感材料：用完即清，禁止打印
    http_sink_t sink = { .buf = s_http_buf, .len = 0 };
    err = http_json(HTTP_METHOD_POST, "/passport/auth", NULL, body, &sink);
    memset(body, 0, sizeof(body));
    memset(sig, 0, sizeof(sig));
    if (err != ESP_OK) goto out;

    jsmntok_t toks[JSMN_TOKENS];
    int n = json_parse(s_http_buf, toks, JSMN_TOKENS);
    if (n < 1 || toks[0].type != JSMN_OBJECT) {
        err = ESP_ERR_INVALID_RESPONSE;
        goto out;
    }
    int ti = json_find(s_http_buf, toks, n, 0, "token");
    if (ti < 0) {
        err = ESP_ERR_INVALID_RESPONSE;
        goto out;
    }
    char token[96];
    json_str(s_http_buf, &toks[ti], token, sizeof(token));
    passport_store_set_string(PASSPORT_KEY_PP_TOKEN, token);

    int bi = json_find(s_http_buf, toks, n, 0, "points_balance");
    if (bi >= 0) {
        passport_store_set_i32(PASSPORT_KEY_BALANCE, json_int(s_http_buf, &toks[bi]));
    }
    // student 子对象：name / student_id / avatar_url
    int si = json_find(s_http_buf, toks, n, 0, "student");
    if (si >= 0 && toks[si].type == JSMN_OBJECT) {
        int sn = json_find(s_http_buf, toks, n, si, "name");
        if (sn >= 0) {
            char name[32];
            json_str(s_http_buf, &toks[sn], name, sizeof(name));
            passport_store_set_string(PASSPORT_KEY_NAME, name);
        }
        int ss = json_find(s_http_buf, toks, n, si, "student_id");
        if (ss >= 0) {
            char sid[32];
            json_str(s_http_buf, &toks[ss], sid, sizeof(sid));
            // 服务端返回的学生 ID 必须与 pid 一致，否则停止同步（文档 7.2 节）
            if (strcmp(sid, pid) != 0) {
                ESP_LOGE(TAG, "student_id 与 pid 不一致，停止同步");
                err = ESP_ERR_INVALID_RESPONSE;
                goto out;
            }
            passport_store_set_string(PASSPORT_KEY_STUDENT_ID, sid);
        }
        int sa = json_find(s_http_buf, toks, n, si, "avatar_url");
        if (sa >= 0 && toks[sa].type == JSMN_STRING) {
            char avatar[160];
            json_str(s_http_buf, &toks[sa], avatar, sizeof(avatar));
            passport_store_set_string(PASSPORT_KEY_AVATAR_URL, avatar);
        }
    }
out:
    memset(secret, 0, sizeof(secret));
    return err;
}

// 带 Bearer 的调用；401 时重 auth 并重试一次
static esp_err_t http_authed(esp_http_client_method_t method, const char *path,
                             const char *body, http_sink_t *sink)
{
    char token[96];
    if (passport_store_get_string(PASSPORT_KEY_PP_TOKEN, token, sizeof(token)) != ESP_OK ||
        token[0] == '\0') {
        esp_err_t err = do_auth();
        if (err != ESP_OK) return err;
        if (passport_store_get_string(PASSPORT_KEY_PP_TOKEN, token, sizeof(token)) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    esp_err_t err = http_json(method, path, token, body, sink);
    if (err == ESP_ERR_INVALID_STATE) {   // 401：token 失效
        err = do_auth();
        if (err != ESP_OK) return err;
        if (passport_store_get_string(PASSPORT_KEY_PP_TOKEN, token, sizeof(token)) != ESP_OK) {
            return ESP_FAIL;
        }
        err = http_json(method, path, token, body, sink);
    }
    return err;
}

static void stamp_balance_time(void)
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    char stamp[20];
    snprintf(stamp, sizeof(stamp), "%02d-%02d %02d:%02d",
             tm_now.tm_mon + 1, tm_now.tm_mday, tm_now.tm_hour, tm_now.tm_min);
    passport_store_set_string(PASSPORT_KEY_BALANCE_TS, stamp);
}

static void cache_balance(int32_t balance)
{
    passport_store_set_i32(PASSPORT_KEY_BALANCE, balance);
    stamp_balance_time();
}

// /passport/me：刷新姓名/余额/在馆/头像缓存
static esp_err_t do_me(void)
{
    http_sink_t sink = { .buf = s_http_buf, .len = 0 };
    esp_err_t err = http_authed(HTTP_METHOD_GET, "/passport/me", NULL, &sink);
    if (err != ESP_OK) return err;

    jsmntok_t toks[JSMN_TOKENS];
    int n = json_parse(s_http_buf, toks, JSMN_TOKENS);
    if (n < 1 || toks[0].type != JSMN_OBJECT) return ESP_ERR_INVALID_RESPONSE;

    int bi = json_find(s_http_buf, toks, n, 0, "points_balance");
    if (bi >= 0) cache_balance(json_int(s_http_buf, &toks[bi]));
    int ni = json_find(s_http_buf, toks, n, 0, "name");
    if (ni >= 0) {
        char name[32];
        json_str(s_http_buf, &toks[ni], name, sizeof(name));
        passport_store_set_string(PASSPORT_KEY_NAME, name);
    }
    int ii = json_find(s_http_buf, toks, n, 0, "in_lab");
    if (ii >= 0) {
        uint8_t in_lab = (uint8_t)(json_int(s_http_buf, &toks[ii]) ? 1 : 0);
        passport_store_set_u8(PASSPORT_KEY_IN_LAB, in_lab);
    }
    int ai = json_find(s_http_buf, toks, n, 0, "avatar_url");
    if (ai >= 0 && toks[ai].type == JSMN_STRING) {
        char avatar[160];
        json_str(s_http_buf, &toks[ai], avatar, sizeof(avatar));
        passport_store_set_string(PASSPORT_KEY_AVATAR_URL, avatar);
    }
    return ESP_OK;
}

// /passport/points/history?limit=3
static esp_err_t do_history(void)
{
    http_sink_t sink = { .buf = s_http_buf, .len = 0 };
    esp_err_t err = http_authed(HTTP_METHOD_GET, "/passport/points/history?limit=3",
                                NULL, &sink);
    if (err != ESP_OK) return err;

    jsmntok_t toks[JSMN_TOKENS];
    int n = json_parse(s_http_buf, toks, JSMN_TOKENS);
    if (n < 1) return ESP_ERR_INVALID_RESPONSE;

    // 响应可能是顶层数组，也可能是 {"items":[...]}；两种都接住
    int arr = 0;
    if (toks[0].type == JSMN_OBJECT) {
        arr = json_find(s_http_buf, toks, n, 0, "items");
        if (arr < 0) arr = json_find(s_http_buf, toks, n, 0, "history");
        if (arr < 0) return ESP_OK;
    }
    if (toks[arr].type != JSMN_ARRAY) return ESP_ERR_INVALID_RESPONSE;

    if (xSemaphoreTake(s_history_lock, pdMS_TO_TICKS(100)) != pdTRUE) return ESP_OK;
    s_history_count = 0;
    int idx = arr + 1;
    int items = toks[arr].size;
    for (int i = 0; i < items && i < PASSPORT_HISTORY_MAX && idx < n; i++) {
        if (toks[idx].type != JSMN_OBJECT) break;
        int di = json_find(s_http_buf, toks, n, idx, "delta");
        if (di < 0) di = json_find(s_http_buf, toks, n, idx, "points");
        int ri = json_find(s_http_buf, toks, n, idx, "reason");
        if (ri < 0) ri = json_find(s_http_buf, toks, n, idx, "title");
        s_history[i].delta = di >= 0 ? json_int(s_http_buf, &toks[di]) : 0;
        s_history[i].text[0] = '\0';
        if (ri >= 0 && toks[ri].type == JSMN_STRING) {
            json_str(s_http_buf, &toks[ri], s_history[i].text, sizeof(s_history[i].text));
        }
        s_history_count++;
        idx += tok_skip(toks, idx);   // 跳过整个元素子树到下一个数组元素
    }
    xSemaphoreGive(s_history_lock);
    return ESP_OK;
}

static void today_str(char out[11])
{
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    snprintf(out, 11, "%04d-%02d-%02d",
             tm_now.tm_year + 1900, tm_now.tm_mon + 1, tm_now.tm_mday);
}

// 每日签到（R4，幂等）：本地日期短路 + 服务端幂等双保险
static esp_err_t do_checkin(void)
{
    char today[11];
    today_str(today);
    char last[11] = { 0 };
    passport_store_get_string(PASSPORT_KEY_LAST_CHECKIN, last, sizeof(last));
    if (strcmp(last, today) == 0) {
        post_ui(PASSPORT_UI_EVT_CHECKIN_DUP, 0);
        return ESP_OK;
    }

    http_sink_t sink = { .buf = s_http_buf, .len = 0 };
    esp_err_t err = http_authed(HTTP_METHOD_POST, "/passport/check-in", "{}", &sink);
    if (err != ESP_OK) return err;

    jsmntok_t toks[JSMN_TOKENS];
    int n = json_parse(s_http_buf, toks, JSMN_TOKENS);
    if (n < 1 || toks[0].type != JSMN_OBJECT) return ESP_ERR_INVALID_RESPONSE;

    int ai = json_find(s_http_buf, toks, n, 0, "awarded");
    int32_t awarded = ai >= 0 ? json_int(s_http_buf, &toks[ai]) : 0;
    int bi = json_find(s_http_buf, toks, n, 0, "balance");
    if (bi >= 0) cache_balance(json_int(s_http_buf, &toks[bi]));

    if (awarded > 0) {
        passport_store_set_string(PASSPORT_KEY_LAST_CHECKIN, today);
        post_ui(PASSPORT_UI_EVT_CHECKIN_OK, awarded);
    } else {
        passport_store_set_string(PASSPORT_KEY_LAST_CHECKIN, today);  // already_checked_in 也记本地
        post_ui(PASSPORT_UI_EVT_CHECKIN_DUP, 0);
    }
    return ESP_OK;
}

// 在馆心跳（R13）：仅当前关联 SSID == lab_ssid 时调用
static esp_err_t do_heartbeat(const char *connected_ssid)
{
    char lab[33];
    if (passport_store_get_string(PASSPORT_KEY_LAB_SSID, lab, sizeof(lab)) != ESP_OK ||
        lab[0] == '\0') {
        strlcpy(lab, PASSPORT_DEFAULT_LAB_SSID, sizeof(lab));
    }
    if (!connected_ssid || strcmp(connected_ssid, lab) != 0) {
        ESP_LOGI(TAG, "当前 SSID 非校区网，跳过心跳");
        return ESP_ERR_INVALID_STATE;   // 非错误：只是门控不通过
    }

    http_sink_t sink = { .buf = s_http_buf, .len = 0 };
    esp_err_t err = http_authed(HTTP_METHOD_POST, "/passport/heartbeat", NULL, &sink);
    if (err != ESP_OK) return err;

    jsmntok_t toks[JSMN_TOKENS];
    int n = json_parse(s_http_buf, toks, JSMN_TOKENS);
    if (n < 1 || toks[0].type != JSMN_OBJECT) return ESP_ERR_INVALID_RESPONSE;

    int ii = json_find(s_http_buf, toks, n, 0, "in_lab");
    if (ii >= 0) {
        uint8_t in_lab = (uint8_t)(json_int(s_http_buf, &toks[ii]) ? 1 : 0);
        passport_store_set_u8(PASSPORT_KEY_IN_LAB, in_lab);
    }
    int bi = json_find(s_http_buf, toks, n, 0, "points_balance");
    if (bi >= 0) cache_balance(json_int(s_http_buf, &toks[bi]));   // 顺带刷新余额缓存
    return ESP_OK;
}

// ---------------------------------------------------------------------------
// 突发主流程
// ---------------------------------------------------------------------------
static void run_burst(passport_net_req_t req)
{
    s_busy = true;
    s_http_buf = malloc(HTTP_BUF_SIZE);
    if (!s_http_buf) {
        post_ui(PASSPORT_UI_EVT_NET_IDLE, 0);
        s_busy = false;
        return;
    }

    char ssid[33] = { 0 };
    esp_err_t err = wifi_burst_connect(ssid, sizeof(ssid));
    if (err != ESP_OK) {
        if (req == PASSPORT_NET_REQ_SYNC) post_ui(PASSPORT_UI_EVT_SYNC_FAIL, 0);
        if (req == PASSPORT_NET_REQ_CHECKIN) post_ui(PASSPORT_UI_EVT_CHECKIN_FAIL, 0);
        goto done;
    }

    // 时间必须已校准才能做 auth（401 重签依赖正确 ts，实现文档 6.3 节）
    if (sntp_sync_if_needed() != ESP_OK && !time_is_valid_now()) {
        ESP_LOGW(TAG, "SNTP 校时失败，本轮只尝试免签名操作");
        if (req == PASSPORT_NET_REQ_SYNC) post_ui(PASSPORT_UI_EVT_SYNC_FAIL, 0);
        if (req == PASSPORT_NET_REQ_CHECKIN) post_ui(PASSPORT_UI_EVT_CHECKIN_FAIL, 0);
        goto done;
    }

    switch (req) {
    case PASSPORT_NET_REQ_SYNC:
        err = do_me();
        if (err == ESP_OK) do_history();
        post_ui(err == ESP_OK ? PASSPORT_UI_EVT_SYNC_OK : PASSPORT_UI_EVT_SYNC_FAIL, 0);
        break;
    case PASSPORT_NET_REQ_CHECKIN:
        err = do_checkin();
        if (err != ESP_OK) post_ui(PASSPORT_UI_EVT_CHECKIN_FAIL, 0);
        break;
    case PASSPORT_NET_REQ_HEARTBEAT: {
        err = do_heartbeat(ssid);
        if (err == ESP_OK) {
            post_ui(PASSPORT_UI_EVT_HEARTBEAT_OK, 0);
        } else if (err != ESP_ERR_INVALID_STATE) {
            // 真正失败：按文档重试一次（INVALID_STATE 只是门控不通过，不重试）
            ESP_LOGW(TAG, "心跳失败，重试一次");
            err = do_heartbeat(ssid);
            if (err == ESP_OK) post_ui(PASSPORT_UI_EVT_HEARTBEAT_OK, 0);
        }
        break;
    }
    default:
        break;
    }

done:
    wifi_stack_stop();
    free(s_http_buf);
    s_http_buf = NULL;
    post_ui(PASSPORT_UI_EVT_NET_IDLE, 0);
    s_busy = false;
}

static void net_task(void *arg)
{
    (void)arg;
    for (;;) {
        passport_net_req_t req = 0;
        if (xQueueReceive(s_req_queue, &req, portMAX_DELAY) != pdTRUE) continue;
        // 合并队列里堆积的同类请求，一次突发办完最新的
        passport_net_req_t extra;
        while (xQueueReceive(s_req_queue, &extra, 0) == pdTRUE) {
            if (extra != req) {
                // 不同请求按优先级覆盖：CHECKIN > SYNC > HEARTBEAT
                if (extra == PASSPORT_NET_REQ_CHECKIN) req = extra;
                else if (extra == PASSPORT_NET_REQ_SYNC && req == PASSPORT_NET_REQ_HEARTBEAT) req = extra;
            }
        }
        if (!passport_store_is_provisioned()) {
            ESP_LOGW(TAG, "设备未激活，忽略网络请求 %d", (int)req);
            post_ui(PASSPORT_UI_EVT_SYNC_FAIL, 0);
            continue;
        }
        run_burst(req);
    }
}

// ---------------------------------------------------------------------------
// 公开 API
// ---------------------------------------------------------------------------
void passport_net_init(QueueHandle_t ui_queue)
{
    s_ui_queue = ui_queue;
    s_req_queue = xQueueCreate(NET_REQ_QUEUE_LEN, sizeof(passport_net_req_t));
    s_wifi_events = xEventGroupCreate();
    s_history_lock = xSemaphoreCreateMutex();
    xTaskCreate(net_task, "smpass_net", NET_TASK_STACK, NULL, 4, &s_task);
}

void passport_net_request(passport_net_req_t req)
{
    if (s_req_queue) xQueueSend(s_req_queue, &req, 0);
}
