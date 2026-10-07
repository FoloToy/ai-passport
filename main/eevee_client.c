#include "eevee_client.h"
#include "eevee_parser.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "eevee_client";
#define HTTP_RESP_BUFFER_SIZE 1024

typedef struct {
    char *buf;
    size_t size;
    size_t len;
} http_resp_buffer_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    http_resp_buffer_t *resp = (http_resp_buffer_t *)evt->user_data;
    if (evt->event_id == HTTP_EVENT_ON_DATA) {
        if (resp && resp->buf && resp->len + evt->data_len < resp->size) {
            memcpy(resp->buf + resp->len, evt->data, evt->data_len);
            resp->len += evt->data_len;
            resp->buf[resp->len] = '\0';
        }
    }
    return ESP_OK;
}

static bool http_request_sync(const eevee_config_t *cfg,
                             const char *path,
                             esp_http_client_method_t method,
                             const char *req_body,
                             char *out_resp, size_t out_resp_size)
{
    if (!cfg || !path || !out_resp || out_resp_size == 0) return false;
    out_resp[0] = '\0';

    char url[256];
    snprintf(url, sizeof(url), "%s%s", cfg->base_url, path);

    http_resp_buffer_t resp_ctx = {
        .buf = out_resp,
        .size = out_resp_size,
        .len = 0,
    };

    esp_http_client_config_t config = {
        .url = url,
        .method = method,
        .timeout_ms = 5000,
        .event_handler = http_event_handler,
        .user_data = &resp_ctx,
        .buffer_size = 512,
        .buffer_size_tx = 512,
        .disable_auto_redirect = true,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "Failed to init HTTP client");
        return false;
    }

    // 设置统一认证 Header (Bearer PAT)
    char auth_header[128];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", cfg->pat_token);
    esp_http_client_set_header(client, "Authorization", auth_header);
    esp_http_client_set_header(client, "Accept", "application/json");

    if (req_body && (method == HTTP_METHOD_POST || method == HTTP_METHOD_PUT)) {
        esp_http_client_set_header(client, "Content-Type", "application/json");
        esp_http_client_set_post_field(client, req_body, strlen(req_body));
    }

    esp_err_t err = esp_http_client_perform(client);
    bool success = false;
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        if (status >= 200 && status < 300) {
            ESP_LOGI(TAG, "HTTP %s %s -> %d (%zu bytes)",
                     (method == HTTP_METHOD_POST) ? "POST" : "GET", path, status, resp_ctx.len);
            success = true;
        } else {
            ESP_LOGW(TAG, "HTTP %s %s returned status %d",
                     (method == HTTP_METHOD_POST) ? "POST" : "GET", path, status);
        }
    } else {
        ESP_LOGE(TAG, "HTTP perform failed: %s (%s)", esp_err_to_name(err), url);
    }

    esp_http_client_cleanup(client);
    return success;
}

bool eevee_client_fetch_me(const eevee_config_t *cfg, eevee_profile_t *out_profile)
{
    char resp_buf[HTTP_RESP_BUFFER_SIZE];
    if (!http_request_sync(cfg, "/api/iot/me", HTTP_METHOD_GET, NULL,
                           resp_buf, sizeof(resp_buf))) {
        return false;
    }
    return eevee_parse_me(resp_buf, out_profile);
}

bool eevee_client_fetch_tasks(const eevee_config_t *cfg, int offset, eevee_task_t *out_task)
{
    char path[64];
    snprintf(path, sizeof(path), "/api/iot/tasks?offset=%d", offset);

    char resp_buf[HTTP_RESP_BUFFER_SIZE];
    if (!http_request_sync(cfg, path, HTTP_METHOD_GET, NULL,
                           resp_buf, sizeof(resp_buf))) {
        return false;
    }
    return eevee_parse_tasks(resp_buf, out_task);
}

bool eevee_client_execute_task_action(const eevee_config_t *cfg,
                                      const char *app_id,
                                      const char *record_id,
                                      const char *action_id,
                                      const char *comment)
{
    char body[256];
    if (!eevee_build_task_action_body(app_id, record_id, action_id, comment,
                                      body, sizeof(body))) {
        return false;
    }

    char resp_buf[256];
    return http_request_sync(cfg, "/api/iot/tasks/action", HTTP_METHOD_POST,
                             body, resp_buf, sizeof(resp_buf));
}

bool eevee_client_fetch_notifications(const eevee_config_t *cfg,
                                      eevee_notification_t *out_notif)
{
    char resp_buf[HTTP_RESP_BUFFER_SIZE];
    if (!http_request_sync(cfg, "/api/iot/notifications/unread", HTTP_METHOD_GET, NULL,
                           resp_buf, sizeof(resp_buf))) {
        return false;
    }
    return eevee_parse_notifications(resp_buf, out_notif);
}

bool eevee_client_mark_notifications_read(const eevee_config_t *cfg,
                                          const char *notif_id_or_all)
{
    char body[128];
    if (!eevee_build_mark_read_body(notif_id_or_all, body, sizeof(body))) {
        return false;
    }

    char resp_buf[128];
    return http_request_sync(cfg, "/api/iot/notifications/read", HTTP_METHOD_POST,
                             body, resp_buf, sizeof(resp_buf));
}

bool eevee_client_fetch_latest_record(const eevee_config_t *cfg,
                                      const char *app_id,
                                      eevee_record_t *out_record)
{
    if (!app_id || app_id[0] == '\0') return false;
    char path[128];
    snprintf(path, sizeof(path), "/api/iot/apps/%s/records/latest", app_id);

    char resp_buf[HTTP_RESP_BUFFER_SIZE];
    if (!http_request_sync(cfg, path, HTTP_METHOD_GET, NULL,
                           resp_buf, sizeof(resp_buf))) {
        return false;
    }
    return eevee_parse_latest_record(resp_buf, out_record);
}

bool eevee_client_report_record(const eevee_config_t *cfg,
                                const char *app_id,
                                float temp, float hum, float battery_volt)
{
    if (!app_id || app_id[0] == '\0') return false;
    char path[128];
    snprintf(path, sizeof(path), "/api/iot/apps/%s/records", app_id);

    char body[128];
    if (!eevee_build_record_body(temp, hum, battery_volt, body, sizeof(body))) {
        return false;
    }

    char resp_buf[256];
    return http_request_sync(cfg, path, HTTP_METHOD_POST,
                             body, resp_buf, sizeof(resp_buf));
}
