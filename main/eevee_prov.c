#include "eevee_prov.h"
#include "eevee_config.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "eevee_prov";

static bool s_running = false;
static httpd_handle_t s_httpd = NULL;
static esp_netif_t *s_ap_netif = NULL;
static eevee_prov_cb_t s_cb = NULL;
static eevee_config_t s_active_cfg;
static eevee_config_t s_candidate_cfg;

static volatile bool s_sta_connected = false;
static volatile bool s_sta_failed = false;
static char s_sta_ip_str[20] = {0};

static TaskHandle_t s_dns_task_handle = NULL;
static int s_dns_socket = -1;

static const char PROV_HTML_PART1[] =
"<!DOCTYPE html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>"
"<title>EEVEE 智能工牌配网</title>"
"<style>"
"*{box-sizing:border-box;margin:0;padding:0;}"
"body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0f172a;color:#f8fafc;padding:16px;display:flex;justify-content:center;}"
".card{background:#1e293b;border-radius:16px;padding:20px;width:100%;max-width:360px;box-shadow:0 10px 25px rgba(0,0,0,0.5);border:1px solid #334155;}"
"h1{font-size:19px;font-weight:700;color:#38bdf8;margin-bottom:4px;text-align:center;}"
".sub{font-size:12px;color:#94a3b8;margin-bottom:16px;text-align:center;}"
".group{margin-bottom:12px;}"
"label{display:block;font-size:12px;font-weight:600;color:#cbd5e1;margin-bottom:4px;}"
"input,select{width:100%;padding:9px 12px;background:#0f172a;border:1px solid #334155;border-radius:8px;color:#fff;font-size:14px;outline:none;}"
"input:focus,select:focus{border-color:#38bdf8;}"
".btn{width:100%;padding:12px;background:#2563eb;color:#fff;border:none;border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;margin-top:6px;}"
".btn:active{background:#1d4ed8;}"
".btn:disabled{background:#475569;cursor:not-allowed;}"
"#msg{margin-top:14px;font-size:13px;text-align:center;display:none;padding:12px;border-radius:8px;line-height:1.5;}"
".info{background:#0369a1;color:#e0f2fe;border:1px solid #0284c7;}"
".success{background:#166534;color:#dcfce7;border:1px solid #22c55e;}"
".error{background:#991b1b;color:#fee2e2;border:1px solid #ef4444;}"
"</style></head><body>"
"<div class='card'>"
"<h1>EEVEE 智能工牌</h1>"
"<p class='sub'>无线网络与服务配置</p>"
"<form id='pform' onsubmit='return submitForm(event);'>"
"<div class='group'>"
"<div style='display:flex;justify-content:space-between;align-items:center;margin-bottom:4px;'>"
"<label style='margin:0;'>选择 Wi-Fi (2.4GHz)</label>"
"<a href='javascript:void(0)' onclick='loadWifiList()' style='font-size:12px;color:#38bdf8;text-decoration:none;'>🔄 刷新扫描</a>"
"</div>"
"<select id='ssid_select' onchange='onSelectSSID(this)'>"
"<option value=''>⏳ 正在扫描附近 Wi-Fi...</option>"
"</select>"
"<input type='text' id='ssid_custom' placeholder='手动输入隐藏 Wi-Fi 名称' style='display:none;margin-top:6px;'>"
"</div>"
"<div class='group'><label>Wi-Fi 密码</label><input type='password' id='pass' placeholder='无密码可留空'></div>"
"<div class='group'><label>服务端基地址 (Base URL)</label><input type='text' id='server' value='";

static const char PROV_HTML_PART2[] =
"' required></div><div class='group'><label>个人访问令牌 (PAT)</label><input type='text' id='pat' value='";

static const char PROV_HTML_PART3[] =
"' required></div><div class='group'><label>业务看板应用 ID</label><input type='text' id='appid' value='";

static const char PROV_HTML_PART4[] =
"' required></div>"
"<button type='submit' class='btn' id='sbtn' onclick='return submitForm(event);'>保存并连接</button>"
"</form><div id='msg'></div></div>"
"<script>"
"function loadWifiList(){"
"var s=document.getElementById('ssid_select'),c=document.getElementById('ssid_custom');"
"s.innerHTML=\"<option value=''>⏳ 正在扫描附近 Wi-Fi...</option>\";"
"fetch('/scan').then(function(r){return r.json();}).then(function(list){"
"s.innerHTML=\"<option value=''>-- 点击选择 Wi-Fi --</option>\";"
"if(list&&list.length>0){"
"list.forEach(function(item){"
"var o=document.createElement('option');"
"o.value=item.ssid;"
"var sig='📶 极好';"
"if(item.rssi<-80)sig='📶 较弱';else if(item.rssi<-65)sig='📶 良好';"
"o.textContent=item.ssid+' ('+sig+')';"
"s.appendChild(o);"
"});"
"}else{"
"s.innerHTML=\"<option value=''>未扫描到 2.4G Wi-Fi，请手动输入</option>\";"
"c.style.display='block';"
"}"
"var m=document.createElement('option');m.value='__manual__';m.textContent='✍️ 手动输入其他 Wi-Fi...';"
"s.appendChild(m);"
"}).catch(function(){"
"s.innerHTML=\"<option value='__manual__'>扫描未完成，请点击手动输入</option>\";"
"c.style.display='block';"
"});"
"}"
"function onSelectSSID(sel){"
"var c=document.getElementById('ssid_custom');"
"if(sel.value==='__manual__'){c.style.display='block';c.focus();}"
"else{c.style.display='none';}"
"}"
"function submitForm(e){"
"if(e&&e.preventDefault)e.preventDefault();"
"var sel=document.getElementById('ssid_select'),c=document.getElementById('ssid_custom');"
"var ssid=sel.value;"
"if(ssid==='__manual__'||!ssid){ssid=c.value.trim();}"
"if(!ssid){alert('请选择或输入 Wi-Fi 名称！');return false;}"
"var msg=document.getElementById('msg'),sbtn=document.getElementById('sbtn');"
"sbtn.disabled=true;msg.className='info';msg.style.display='block';msg.innerText='正在保存配置到工牌...';"
"var data={"
"ssid:ssid,"
"pass:document.getElementById('pass').value,"
"server:document.getElementById('server').value.trim(),"
"pat:document.getElementById('pat').value.trim(),"
"appid:document.getElementById('appid').value.trim()"
"};"
"fetch('/setup',{"
"method:'POST',"
"headers:{'Content-Type':'application/json'},"
"body:JSON.stringify(data)"
"})"
".then(function(r){if(!r.ok)throw new Error('HTTP '+r.status);return r.json();})"
".then(function(res){"
"msg.className='success';"
"msg.innerHTML='<strong>✅ 配置已成功保存！</strong><br>设备正在连接网络，热点可能断开。<br>请观察工牌屏幕上的联网状态。';"
"sbtn.innerText='已保存';"
"var attempts=0;"
"var timer=setInterval(function(){"
"attempts++;"
"fetch('/status').then(function(r){return r.json();}).then(function(st){"
"if(st.connected){clearInterval(timer);msg.className='success';msg.innerHTML='<strong>🎉 恭喜！工牌已成功联网！</strong><br>已获取 IP: '+st.ip+'<br>热点已关闭，工牌已切换到在线模式。';}"
"else if(attempts>=12){clearInterval(timer);msg.className='info';msg.innerHTML='<strong>ℹ️ 配置已保存，连接中...</strong><br>若长时间未连上，请核对密码后重新提交。';sbtn.disabled=false;sbtn.innerText='重新保存';}"
"}).catch(function(){});"
"},2000);"
"}).catch(function(err){"
"msg.className='error';msg.innerText='保存请求失败: '+err.message+'，请确保已连接工牌热点后重试。';sbtn.disabled=false;"
"});"
"return false;"
"}"
"window.onload=function(){loadWifiList();};"
"</script></body></html>";

/*-----------------------------------------------------------------------------
 * 1. 极简 Captive Portal DNS 服务 (UDP 53)
 *----------------------------------------------------------------------------*/
static void dns_server_task(void *pvParam)
{
    (void)pvParam;
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(53);

    s_dns_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (s_dns_socket < 0) {
        ESP_LOGE(TAG, "Failed to create DNS socket");
        vTaskDelete(NULL);
        return;
    }

    struct timeval tv = { .tv_sec = 2, .tv_usec = 0 };
    setsockopt(s_dns_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    if (bind(s_dns_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind DNS socket");
        close(s_dns_socket);
        s_dns_socket = -1;
        vTaskDelete(NULL);
        return;
    }

    ESP_LOGI(TAG, "Captive DNS server running on port 53");
    uint8_t buffer[256];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    while (s_running) {
        int len = recvfrom(s_dns_socket, buffer, sizeof(buffer), 0,
                           (struct sockaddr *)&client_addr, &client_len);
        if (len < 12) continue; // DNS header size is 12

        // 构造 DNS 响应：统一返回 AP 地址 192.168.4.1
        buffer[2] |= 0x84; // QR=1, AA=1
        buffer[3] |= 0x80; // RA=1
        buffer[7] = 1;    // Answers count = 1

        // 追加 Answer 字段 (Type A, Class IN, TTL 60, IP 192.168.4.1)
        uint8_t *p = buffer + len;
        *p++ = 0xc0; *p++ = 0x0c; // Name offset 12
        *p++ = 0x00; *p++ = 0x01; // Type A
        *p++ = 0x00; *p++ = 0x01; // Class IN
        *p++ = 0x00; *p++ = 0x00; *p++ = 0x00; *p++ = 0x3c; // TTL 60s
        *p++ = 0x00; *p++ = 0x04; // Data length 4
        *p++ = 192;  *p++ = 168;  *p++ = 4;    *p++ = 1;    // IP: 192.168.4.1

        sendto(s_dns_socket, buffer, p - buffer, 0,
               (struct sockaddr *)&client_addr, client_len);
    }

    if (s_dns_socket >= 0) {
        close(s_dns_socket);
        s_dns_socket = -1;
    }
    vTaskDelete(NULL);
}

/*-----------------------------------------------------------------------------
 * 2. HTTP 页面与 API 接口处理器
 *----------------------------------------------------------------------------*/
static esp_err_t prov_root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send_chunk(req, PROV_HTML_PART1, HTTPD_RESP_USE_STRLEN);

    const char *srv = s_active_cfg.base_url[0] ? s_active_cfg.base_url : EEVEE_DEFAULT_BASE_URL;
    httpd_resp_send_chunk(req, srv, HTTPD_RESP_USE_STRLEN);

    httpd_resp_send_chunk(req, PROV_HTML_PART2, HTTPD_RESP_USE_STRLEN);

    const char *pat = s_active_cfg.pat_token[0] ? s_active_cfg.pat_token : EEVEE_DEFAULT_PAT_TOKEN;
    httpd_resp_send_chunk(req, pat, HTTPD_RESP_USE_STRLEN);

    httpd_resp_send_chunk(req, PROV_HTML_PART3, HTTPD_RESP_USE_STRLEN);

    const char *app = s_active_cfg.app_id[0] ? s_active_cfg.app_id : EEVEE_DEFAULT_APP_ID;
    httpd_resp_send_chunk(req, app, HTTPD_RESP_USE_STRLEN);

    httpd_resp_send_chunk(req, PROV_HTML_PART4, HTTPD_RESP_USE_STRLEN);
    return httpd_resp_send_chunk(req, NULL, 0);
}

static esp_err_t prov_captive_redirect_handler(httpd_req_t *req)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t prov_404_handler(httpd_req_t *req, httpd_err_code_t err)
{
    (void)err;
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

static void prov_connect_task(void *param)
{
    (void)param;
    // 延迟 600ms，确保 HTTP 响应包在 TCP 链路上完整交付手机浏览器，防止单射频跳信道中断 TCP
    vTaskDelay(pdMS_TO_TICKS(600));

    ESP_LOGI(TAG, "Applying credentials and connecting to '%s'...", s_candidate_cfg.wifi_ssid);
    wifi_config_t wifi_cfg;
    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    strncpy((char *)wifi_cfg.sta.ssid, s_candidate_cfg.wifi_ssid, sizeof(wifi_cfg.sta.ssid) - 1);
    strncpy((char *)wifi_cfg.sta.password, s_candidate_cfg.wifi_pass, sizeof(wifi_cfg.sta.password) - 1);

    esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    esp_wifi_disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_wifi_connect();

    vTaskDelete(NULL);
}

static esp_err_t prov_setup_post_handler(httpd_req_t *req)
{
    if (req->content_len > 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
        return ESP_FAIL;
    }

    char body[1025] = {0};
    int ret = httpd_req_recv(req, body, req->content_len);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Recv failed");
        return ESP_FAIL;
    }

    cJSON *root = cJSON_Parse(body);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *j_ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *j_pass = cJSON_GetObjectItem(root, "pass");
    cJSON *j_server = cJSON_GetObjectItem(root, "server");
    cJSON *j_pat = cJSON_GetObjectItem(root, "pat");
    cJSON *j_appid = cJSON_GetObjectItem(root, "appid");

    if (!j_ssid || !j_ssid->valuestring || j_ssid->valuestring[0] == '\0') {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID cannot be empty");
        return ESP_FAIL;
    }

    memset(&s_candidate_cfg, 0, sizeof(s_candidate_cfg));
    strncpy(s_candidate_cfg.wifi_ssid, j_ssid->valuestring, sizeof(s_candidate_cfg.wifi_ssid) - 1);
    if (j_pass && j_pass->valuestring) {
        strncpy(s_candidate_cfg.wifi_pass, j_pass->valuestring, sizeof(s_candidate_cfg.wifi_pass) - 1);
    }
    if (j_server && j_server->valuestring && j_server->valuestring[0] != '\0') {
        strncpy(s_candidate_cfg.base_url, j_server->valuestring, sizeof(s_candidate_cfg.base_url) - 1);
    } else {
        strncpy(s_candidate_cfg.base_url, EEVEE_DEFAULT_BASE_URL, sizeof(s_candidate_cfg.base_url) - 1);
    }
    if (j_pat && j_pat->valuestring && j_pat->valuestring[0] != '\0') {
        strncpy(s_candidate_cfg.pat_token, j_pat->valuestring, sizeof(s_candidate_cfg.pat_token) - 1);
    } else {
        strncpy(s_candidate_cfg.pat_token, EEVEE_DEFAULT_PAT_TOKEN, sizeof(s_candidate_cfg.pat_token) - 1);
    }
    if (j_appid && j_appid->valuestring && j_appid->valuestring[0] != '\0') {
        strncpy(s_candidate_cfg.app_id, j_appid->valuestring, sizeof(s_candidate_cfg.app_id) - 1);
    } else {
        strncpy(s_candidate_cfg.app_id, EEVEE_DEFAULT_APP_ID, sizeof(s_candidate_cfg.app_id) - 1);
    }
    s_candidate_cfg.poll_interval_sec = 45;

    cJSON_Delete(root);

    // 核心保证：接收到配置后立即写入 Flash NVS！确保即便手机端断开连接，配置也绝不丢失
    bool saved_ok = eevee_config_save(&s_candidate_cfg);
    s_active_cfg = s_candidate_cfg;
    ESP_LOGI(TAG, "Config saved to Flash NVS: SSID='%s', Server='%s', Result=%s",
             s_candidate_cfg.wifi_ssid, s_candidate_cfg.base_url, saved_ok ? "SUCCESS" : "FAILED");

    s_sta_connected = false;
    s_sta_failed = false;

    // 先发送 HTTP 200 确认给客户端，让手机浏览器立即展示“配置已保存”反馈
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"saved\",\"message\":\"配置已成功保存！\"}", HTTPD_RESP_USE_STRLEN);

    // 延迟发起 STA 连接，防止单天线 RF 立即跳信道斩断 HTTP 响应传输
    xTaskCreate(prov_connect_task, "prov_conn", 3072, NULL, 5, NULL);
    return ESP_OK;
}

static esp_err_t prov_status_handler(httpd_req_t *req)
{
    char resp[96];
    snprintf(resp, sizeof(resp), "{\"connected\":%s,\"ip\":\"%s\"}",
             s_sta_connected ? "true" : "false",
             s_sta_ip_str);
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t prov_scan_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Scanning for 2.4GHz Wi-Fi access points...");
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
    };
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);

    uint16_t ap_count = 15;
    wifi_ap_record_t ap_records[15];
    memset(ap_records, 0, sizeof(ap_records));

    if (err == ESP_OK) {
        esp_wifi_scan_get_ap_records(&ap_count, ap_records);
    } else {
        ESP_LOGW(TAG, "Scan failed: %s", esp_err_to_name(err));
        ap_count = 0;
    }

    // 冒泡排序：按 RSSI 从强到弱排序
    if (ap_count > 1) {
        for (int i = 0; i < (int)ap_count - 1; i++) {
            for (int j = 0; j < (int)ap_count - 1 - i; j++) {
                if (ap_records[j].rssi < ap_records[j + 1].rssi) {
                    wifi_ap_record_t tmp = ap_records[j];
                    ap_records[j] = ap_records[j + 1];
                    ap_records[j + 1] = tmp;
                }
            }
        }
    }

    cJSON *root = cJSON_CreateArray();
    for (uint16_t i = 0; i < ap_count; i++) {
        if (ap_records[i].ssid[0] == '\0') continue;

        // 去重
        bool exists = false;
        cJSON *item = NULL;
        cJSON_ArrayForEach(item, root) {
            cJSON *s = cJSON_GetObjectItem(item, "ssid");
            if (s && s->valuestring && strcmp(s->valuestring, (char *)ap_records[i].ssid) == 0) {
                exists = true;
                break;
            }
        }
        if (exists) continue;

        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "ssid", (char *)ap_records[i].ssid);
        cJSON_AddNumberToObject(obj, "rssi", ap_records[i].rssi);
        cJSON_AddItemToArray(root, obj);
    }

    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    httpd_resp_set_type(req, "application/json");
    if (json_str) {
        httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
        free(json_str);
    } else {
        httpd_resp_send(req, "[]", HTTPD_RESP_USE_STRLEN);
    }
    return ESP_OK;
}

static void prov_delayed_stop_task(void *param)
{
    (void)param;
    vTaskDelay(pdMS_TO_TICKS(2500));
    ESP_LOGI(TAG, "Provisioning completed, stopping AP service...");
    if (s_cb) {
        s_cb(&s_candidate_cfg, true);
    }
    eevee_prov_stop();
    vTaskDelete(NULL);
}

/*-----------------------------------------------------------------------------
 * 3. 网络事件监听
 *----------------------------------------------------------------------------*/
static void prov_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        snprintf(s_sta_ip_str, sizeof(s_sta_ip_str), IPSTR, IP2STR(&ev->ip_info.ip));
        s_sta_connected = true;
        s_sta_failed = false;
        ESP_LOGI(TAG, "STA connected successfully! Got IP: %s", s_sta_ip_str);

        // 延迟停止热点，通知主程序
        xTaskCreate(prov_delayed_stop_task, "prov_stop", 3072, NULL, 5, NULL);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        s_sta_connected = false;
        ESP_LOGW(TAG, "STA disconnected event during prov mode");
    }
}

/*-----------------------------------------------------------------------------
 * 4. 外部生命周期 API
 *----------------------------------------------------------------------------*/
bool eevee_prov_start(const eevee_config_t *current_cfg, eevee_prov_cb_t cb)
{
    if (s_running) return true;
    ESP_LOGI(TAG, "Starting SoftAP Web Provisioning service...");

    if (current_cfg) {
        s_active_cfg = *current_cfg;
    } else {
        eevee_config_reset_defaults(&s_active_cfg);
    }
    s_cb = cb;
    s_sta_connected = false;
    s_sta_failed = false;
    s_sta_ip_str[0] = '\0';
    s_running = true;

    // 1. 初始化 SoftAP 网络接口
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
    }

    // 2. 切换 Wi-Fi 模式为 APSTA
    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = "Eevee-Badge-Setup",
            .ssid_len = 0,
            .channel = 1,
            .password = "",
            .max_connection = 1,
            .authmode = WIFI_AUTH_OPEN,
        },
    };
    esp_wifi_set_mode(WIFI_MODE_APSTA);
    esp_wifi_set_config(WIFI_IF_AP, &ap_cfg);
    esp_wifi_start();

    // 注册网络事件
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &prov_event_handler, NULL);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &prov_event_handler, NULL);

    // 3. 启动 Captive DNS 服务
    xTaskCreate(dns_server_task, "dns_server", 3072, NULL, 4, &s_dns_task_handle);

    // 4. 启动轻量 HTTP Server (资源预算: 3 sockets, stack 6144)
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = 80;
    config.max_open_sockets = 3;
    config.stack_size = 6144;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;

    if (httpd_start(&s_httpd, &config) == ESP_OK) {
        httpd_uri_t uri_root = { .uri = "/", .method = HTTP_GET, .handler = prov_root_handler };
        httpd_register_uri_handler(s_httpd, &uri_root);

        httpd_uri_t uri_setup = { .uri = "/setup", .method = HTTP_POST, .handler = prov_setup_post_handler };
        httpd_register_uri_handler(s_httpd, &uri_setup);

        httpd_uri_t uri_status = { .uri = "/status", .method = HTTP_GET, .handler = prov_status_handler };
        httpd_register_uri_handler(s_httpd, &uri_status);

        httpd_uri_t uri_scan = { .uri = "/scan", .method = HTTP_GET, .handler = prov_scan_handler };
        httpd_register_uri_handler(s_httpd, &uri_scan);

        // 针对 iOS / Android / Windows 的 Captive 探测地址做重定向
        httpd_uri_t uri_apple = { .uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = prov_captive_redirect_handler };
        httpd_register_uri_handler(s_httpd, &uri_apple);

        httpd_uri_t uri_android = { .uri = "/generate_204", .method = HTTP_GET, .handler = prov_captive_redirect_handler };
        httpd_register_uri_handler(s_httpd, &uri_android);

        // 通用 404 捕获并重定向到配网页
        httpd_register_err_handler(s_httpd, HTTPD_404_NOT_FOUND, prov_404_handler);

        ESP_LOGI(TAG, "Provisioning HTTP Server started on 192.168.4.1:80");
        return true;
    }

    ESP_LOGE(TAG, "Failed to start HTTP server");
    eevee_prov_stop();
    return false;
}

void eevee_prov_stop(void)
{
    if (!s_running) return;
    ESP_LOGI(TAG, "Stopping SoftAP Web Provisioning service...");
    s_running = false;

    if (s_httpd) {
        httpd_stop(s_httpd);
        s_httpd = NULL;
    }

    if (s_dns_socket >= 0) {
        close(s_dns_socket);
        s_dns_socket = -1;
    }
    s_dns_task_handle = NULL;

    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &prov_event_handler);
    esp_event_handler_unregister(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &prov_event_handler);

    // 切回纯 STA 模式，释放 AP 热点与对应内部无线缓冲
    esp_wifi_set_mode(WIFI_MODE_STA);
    ESP_LOGI(TAG, "SoftAP stopped. Switched back to WIFI_MODE_STA.");
}

bool eevee_prov_is_running(void)
{
    return s_running;
}
