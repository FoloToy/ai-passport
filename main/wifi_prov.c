// main/wifi_prov.c —— SoftAP 网页配网。
// 流程:设备开热点 "FoloToy-xxxx" -> 手机连上 -> 打开 http://192.168.4.1
//       -> 选/填 WiFi 名称与密码 -> 保存进 NVS -> 设备重启后用新凭据联网。
// 为什么走网页而不是 SmartConfig:不依赖装 App,任何手机浏览器都能用;
// 也不需要在 240x320 的三键界面上输密码。
// 网页文案由手机浏览器渲染,不占设备字形集:glyph-check: ignore-file
#include "wifi_prov.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "net_wifi.h"
#include "prov_form.h"
#include "wifi_store.h"

static const char *TAG = "wifi_prov";

#define PROV_SCAN_MAX 12
#define PROV_PAGE_CAP 4096
#define PROV_BODY_CAP 256

static httpd_handle_t s_server;
static esp_netif_t *s_ap_netif;
static bool s_active;
static char s_ap_ssid[24];
static volatile wifi_prov_state_t s_state;

static wifi_ap_record_t s_scan[PROV_SCAN_MAX];
static int s_scan_n;
static volatile bool s_scan_done;
static volatile bool s_scan_running;

static void scan_kick(void);   // 定义在扫描一节,网页处理器要先用到

wifi_prov_state_t wifi_prov_state(void) {
    return s_state;
}

bool wifi_prov_active(void) {
    return s_active;
}

const char *wifi_prov_ap_ssid(void) {
    return s_ap_ssid;
}

// ---- 网页 ----

static void app(char *buf, size_t cap, size_t *n, const char *fmt, ...) {
    if (*n + 1 >= cap) return;
    va_list ap;
    va_start(ap, fmt);
    int w = vsnprintf(buf + *n, cap - *n, fmt, ap);
    va_end(ap);
    if (w <= 0) return;
    size_t left = cap - *n;
    *n += ((size_t)w < left) ? (size_t)w : (left - 1);
}

// 网络名可能带引号/尖括号,直接拼进 HTML 会破页,先转义。
static void html_escape(char *dst, size_t cap, const char *src) {
    size_t o = 0;
    for (size_t i = 0; src[i] != '\0' && o + 1 < cap; i++) {
        const char *rep = NULL;
        switch (src[i]) {
            case '&': rep = "&amp;"; break;
            case '<': rep = "&lt;"; break;
            case '>': rep = "&gt;"; break;
            case '"': rep = "&quot;"; break;
            case '\'': rep = "&#39;"; break;
            default: break;
        }
        if (rep != NULL) {
            size_t rl = strlen(rep);
            if (o + rl + 1 > cap) break;
            memcpy(dst + o, rep, rl);
            o += rl;
        } else {
            dst[o++] = src[i];
        }
    }
    dst[o] = '\0';
}

static const char PAGE_HEAD[] =
    "<!doctype html><html lang=zh-CN><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>FoloToy 配网</title><style>"
    "body{margin:0;padding:22px;font-family:-apple-system,BlinkMacSystemFont,'PingFang SC',sans-serif;"
    "background:#0E1A30;color:#E8EEF7}"
    ".c{max-width:420px;margin:0 auto}h1{font-size:21px;margin:0 0 4px}"
    ".s{font-size:13px;color:#7C8FA9;margin:0 0 18px}"
    "label{display:block;margin:14px 0 6px;font-size:14px;color:#9DB0CC}"
    "input{width:100%;box-sizing:border-box;padding:12px;border-radius:10px;"
    "border:1px solid #2A3F63;background:#16233B;color:#E8EEF7;font-size:16px}"
    "button{margin-top:22px;width:100%;padding:14px;border:0;border-radius:10px;"
    "background:#1D9E75;color:#fff;font-size:16px;font-weight:600}"
    ".t{font-size:13px;line-height:1.7;color:#7C8FA9;margin-top:16px}"
    ".h{margin:0 0 4px;padding-left:20px;font-size:13px;line-height:1.8;color:#9DB0CC}"
    ".h b{color:#4FC3F7;font-weight:600}"
    ".e{color:#F06B6B;font-size:14px;margin-top:12px}"
    "a{color:#4FC3F7}</style></head><body><div class=c>";

static const char PAGE_TAIL[] = "</div></body></html>";

static char *page_build(void) {
    char *buf = malloc(PROV_PAGE_CAP);
    if (buf == NULL) return NULL;
    size_t n = 0;
    app(buf, PROV_PAGE_CAP, &n, "%s", PAGE_HEAD);
    app(buf, PROV_PAGE_CAP, &n, "<h1>FoloToy 配网</h1><p class=s>设备热点:%s</p>", s_ap_ssid);
    app(buf, PROV_PAGE_CAP, &n,
        "<ol class=h><li><b>选</b>下面列表里你家的 WiFi 名称</li>"
        "<li><b>填</b>这个 WiFi 的密码</li>"
        "<li><b>点保存</b>,设备自动重启并联网</li></ol>");

    // 首次打开时扫描还没结束,先给个自动刷新的过渡页,免得用户对着空列表。
    if (!s_scan_done) {
        app(buf, PROV_PAGE_CAP, &n,
            "<p class=t>正在扫描附近网络,约 3 秒后自动刷新…</p>"
            "<meta http-equiv=refresh content=\"3;url=/\">");
        app(buf, PROV_PAGE_CAP, &n, "%s", PAGE_TAIL);
        return buf;
    }

    app(buf, PROV_PAGE_CAP, &n,
        "<form method=post action=/save>"
        "<label>WiFi 名称(仅 2.4GHz)</label>"
        "<input name=ssid required maxlength=32 autocomplete=off autocapitalize=off "
        "spellcheck=false list=nets>"
        "<datalist id=nets>");
    for (int i = 0; i < s_scan_n; i++) {
        char safe[96];
        html_escape(safe, sizeof(safe), (const char *)s_scan[i].ssid);
        app(buf, PROV_PAGE_CAP, &n, "<option value=\"%s\">", safe);
    }
    app(buf, PROV_PAGE_CAP, &n,
        "</datalist>"
        "<label>密码(WPA2,至少 8 位)</label>"
        "<input name=pass type=password maxlength=63 autocomplete=off>"
        "<button>保存并连接</button></form>"
        "<p class=t>扫描到 %d 个网络。列表里没有就手填,注意区分大小写。"
        "<a href=/rescan>重新扫描</a><br>"
        "只支持 2.4GHz:路由器双频合一通常没问题,单独开了 5G 的那个连不上。<br>"
        "保存后设备会重启并联网;若连不上,等 30 秒重新长按上键再来一次。<br>"
        "设备只连你家路由器,不装 App、不上传任何信息。</p>",
        s_scan_n);
    app(buf, PROV_PAGE_CAP, &n, "%s", PAGE_TAIL);
    return buf;
}

// 错误页/成功页都很短,单独给 1 KB 足够。
static char *page_msg(const char *msg) {
    char *buf = malloc(1024);
    if (buf == NULL) return NULL;
    char safe[256];
    html_escape(safe, sizeof(safe), msg);
    size_t n = 0;
    app(buf, 1024, &n, "%s<h1>没配成功</h1><p class=e>%s</p>"
                       "<p class=t><a href=/>返回重填</a></p>%s",
        PAGE_HEAD, safe, PAGE_TAIL);
    return buf;
}

static char *page_saved(const char *ssid) {
    char *buf = malloc(1024);
    if (buf == NULL) return NULL;
    char safe[96];
    html_escape(safe, sizeof(safe), ssid);
    size_t n = 0;
    app(buf, 1024, &n, "%s<h1>已保存</h1><p class=t>设备正在重启,随后连接 \"%s\"。"
                       "热点会自动关闭,手机可以断开。</p>%s",
        PAGE_HEAD, safe, PAGE_TAIL);
    return buf;
}

static void reboot_task(void *arg) {
    (void)arg;
    vTaskDelay(pdMS_TO_TICKS(1200));   // 留时间把「已保存」页面发完
    esp_restart();
}

static esp_err_t send_page(httpd_req_t *req, const char *status, char *page) {
    if (page == NULL) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        return httpd_resp_send(req, "内存不足", HTTPD_RESP_USE_STRLEN);
    }
    httpd_resp_set_status(req, status);
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    esp_err_t err = httpd_resp_send(req, page, HTTPD_RESP_USE_STRLEN);
    free(page);
    return err;
}

static esp_err_t root_get(httpd_req_t *req) {
    // 扫描放到第一次打开网页时做:此时 WiFi 状态已稳定,不会被开机重连打断。
    if (!s_scan_done) scan_kick();
    return send_page(req, "200 OK", page_build());
}

static esp_err_t save_post(httpd_req_t *req) {
    char body[PROV_BODY_CAP];
    int total = 0;
    while (total < (int)sizeof(body) - 1) {
        int got = httpd_req_recv(req, body + total, sizeof(body) - 1 - (size_t)total);
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (got <= 0) break;
        total += got;
    }
    body[total] = '\0';
    if (total == 0) {
        return send_page(req, "400 Bad Request", page_msg("没有收到表单内容"));
    }

    char ssid[40];
    char pass[72];
    if (!prov_form_parse(body, (size_t)total, ssid, sizeof(ssid), pass, sizeof(pass))) {
        return send_page(req, "400 Bad Request", page_msg("表单解析失败"));
    }
    prov_form_err_t chk = prov_form_check(ssid, pass);
    if (chk != PROV_FORM_OK) {
        return send_page(req, "400 Bad Request", page_msg(prov_form_error_text(chk)));
    }
    if (!wifi_store_save(ssid, pass)) {
        return send_page(req, "500 Internal Server Error", page_msg("写入存储失败,请重试"));
    }

    s_state = WIFI_PROV_SAVED;
    ESP_LOGI(TAG, "已保存 \"%s\",准备重启", ssid);
    esp_err_t err = send_page(req, "200 OK", page_saved(ssid));
    if (xTaskCreate(reboot_task, "prov_reboot", 2560, NULL, 4, NULL) != pdPASS) {
        ESP_LOGW(TAG, "重启任务创建失败,手动重启");
        esp_restart();
    }
    return err;
}

// 扫描失败或刚换了位置时,让页面能手动再来一次。
static esp_err_t rescan_get(httpd_req_t *req) {
    s_scan_done = false;
    scan_kick();
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "/");
    return httpd_resp_send(req, NULL, 0);
}

static const httpd_uri_t URI_ROOT = {.uri = "/", .method = HTTP_GET, .handler = root_get};
static const httpd_uri_t URI_RESCAN = {.uri = "/rescan", .method = HTTP_GET, .handler = rescan_get};
static const httpd_uri_t URI_SAVE = {.uri = "/save", .method = HTTP_POST, .handler = save_post};

// ---- 扫描附近网络 ----

static void scan_task(void *arg) {
    (void)arg;
    wifi_scan_config_t cfg = {0};
    cfg.show_hidden = false;
    cfg.scan_type = WIFI_SCAN_TYPE_ACTIVE;
    cfg.scan_time.active.min = 100;
    cfg.scan_time.active.max = 300;

    s_scan_n = 0;
    // 两个坑都会让扫描白跑:
    //   1) 刚切到 APSTA 时 WiFi 还在重启,scan_start 直接回 ESP_ERR_WIFI_STATE;
    //   2) STA 正在重连时,进行中的扫描会被打断,结果集是 0 个。
    // 所以状态没就绪就等,扫到空也重来,最多 3 轮。
    for (int round = 0; round < 3 && s_scan_n == 0; round++) {
        esp_err_t err = ESP_ERR_WIFI_STATE;
        for (int attempt = 0; attempt < 20; attempt++) {
            err = esp_wifi_scan_start(&cfg, true);
            if (err != ESP_ERR_WIFI_STATE) break;
            vTaskDelay(pdMS_TO_TICKS(300));
        }
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "第 %d 轮扫描失败: %s", round + 1, esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(500));
            continue;
        }
        uint16_t num = 0;
        if (esp_wifi_scan_get_ap_num(&num) != ESP_OK || num == 0) {
            esp_wifi_clear_ap_list();
            vTaskDelay(pdMS_TO_TICKS(800));
            continue;
        }
        uint16_t want = (num > 20) ? 20 : num;
        wifi_ap_record_t *recs = calloc(want, sizeof(wifi_ap_record_t));
        if (recs == NULL) break;
        if (esp_wifi_scan_get_ap_records(&want, recs) == ESP_OK) {
            for (uint16_t i = 0; i < want && s_scan_n < PROV_SCAN_MAX; i++) {
                if (recs[i].ssid[0] == '\0') continue;   // 隐藏 SSID 不列
                bool dup = false;
                for (int k = 0; k < s_scan_n; k++) {
                    if (strcmp((const char *)s_scan[k].ssid, (const char *)recs[i].ssid) == 0) {
                        dup = true;
                        break;
                    }
                }
                if (!dup) s_scan[s_scan_n++] = recs[i];
            }
        }
        free(recs);
    }
    s_scan_done = true;
    s_scan_running = false;
    ESP_LOGI(TAG, "扫描完成,可用网络 %d 个", s_scan_n);
    vTaskDelete(NULL);
}

static void scan_kick(void) {
    if (s_scan_running || s_scan_done) return;
    s_scan_running = true;
    if (xTaskCreate(scan_task, "prov_scan", 3584, NULL, 4, NULL) != pdPASS) {
        s_scan_running = false;
        s_scan_done = true;
    }
}

// ---- 启停 ----

esp_err_t wifi_prov_start(void) {
    if (s_active) return ESP_OK;
    if (!net_wifi_stack_ready()) net_wifi_start();
    if (!net_wifi_stack_ready()) {
        ESP_LOGE(TAG, "网络栈不可用,配网无法开启");
        return ESP_FAIL;
    }

    // 用 STA MAC 命名(就是设备标签上的那个),SoftAP MAC 会 +1,容易和标签对不上。
    uint8_t mac[6] = {0};
    if (esp_read_mac(mac, ESP_MAC_WIFI_STA) != ESP_OK) {
        esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP);
    }
    snprintf(s_ap_ssid, sizeof(s_ap_ssid), "FoloToy-%02X%02X", mac[4], mac[5]);

    if (s_ap_netif == NULL) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (s_ap_netif == NULL) {
            ESP_LOGE(TAG, "创建 AP 接口失败");
            return ESP_FAIL;
        }
    }

    wifi_config_t ap_cfg = {0};
    snprintf((char *)ap_cfg.ap.ssid, sizeof(ap_cfg.ap.ssid), "%s", s_ap_ssid);
    ap_cfg.ap.ssid_len = (uint8_t)strlen(s_ap_ssid);
    ap_cfg.ap.channel = 1;
    ap_cfg.ap.max_connection = 4;
    ap_cfg.ap.authmode = WIFI_AUTH_OPEN;   // 开放热点:手机连上就能开网页,少一步输密码

    // APSTA 而不是纯 AP:STA 侧保持原网络,配网期间开奖页照常显示旧数据。
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_APSTA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动热点失败: %s", esp_err_to_name(err));
        esp_wifi_set_mode(WIFI_MODE_STA);
        return err;
    }

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;
    cfg.max_open_sockets = 3;
    cfg.lru_purge_enable = true;
    cfg.stack_size = 5120;
    if (s_server == NULL) {
        err = httpd_start(&s_server, &cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "网页服务启动失败: %s", esp_err_to_name(err));
            s_server = NULL;
            esp_wifi_set_mode(WIFI_MODE_STA);
            return err;
        }
        httpd_register_uri_handler(s_server, &URI_ROOT);
        httpd_register_uri_handler(s_server, &URI_SAVE);
        httpd_register_uri_handler(s_server, &URI_RESCAN);
    }

    s_state = WIFI_PROV_WAITING;
    s_active = true;
    ESP_LOGI(TAG, "配网已开启:热点 \"%s\" -> http://192.168.4.1", s_ap_ssid);
    return ESP_OK;
}

void wifi_prov_stop(void) {
    if (!s_active) return;
    s_active = false;
    s_state = WIFI_PROV_IDLE;
    if (s_server != NULL) {
        httpd_stop(s_server);
        // httpd_stop 已释放句柄内部资源,但句柄值不会清零,必须手动置空,
        // 否则下次 httpd_start 会拿到野指针。
        s_server = NULL;
    }
    esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_LOGI(TAG, "配网已关闭");
}
