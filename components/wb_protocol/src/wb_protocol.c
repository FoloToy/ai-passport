// components/wb_protocol/src/wb_protocol.c
// WorkBuddy BLE 协议实现
#include "wb_protocol.h"
#include "esp_log.h"
#include "esp_err.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "wb_protocol";

// ============================================================
// 协议消息类型解析
// ============================================================

static wb_msg_type_t parse_msg_type(const char *type_str) {
    if (strcmp(type_str, "wb_show_options") == 0)  return WB_MSG_SHOW_OPTIONS;
    if (strcmp(type_str, "wb_option_selected") == 0) return WB_MSG_OPTION_SELECTED;
    if (strcmp(type_str, "wb_ask") == 0)          return WB_MSG_ASK;
    if (strcmp(type_str, "wb_answer") == 0)       return WB_MSG_ANSWER;
    if (strcmp(type_str, "wb_notification") == 0)  return WB_MSG_NOTIFICATION;
    if (strcmp(type_str, "wb_task_complete") == 0) return WB_MSG_TASK_COMPLETE;
    if (strcmp(type_str, "wb_task_error") == 0)  return WB_MSG_TASK_ERROR;
    if (strcmp(type_str, "wb_status") == 0)        return WB_MSG_STATUS;
    if (strcmp(type_str, "wb_ping") == 0)         return WB_MSG_PING;
    if (strcmp(type_str, "wb_pong") == 0)         return WB_MSG_PONG;
    if (strcmp(type_str, "wb_heartbeat") == 0)    return WB_MSG_HEARTBEAT;
    if (strcmp(type_str, "wb_audio") == 0)        return WB_MSG_AUDIO;
    if (strcmp(type_str, "wb_audio_end") == 0)    return WB_MSG_AUDIO_END;
    if (strcmp(type_str, "wb_debug_talk") == 0)   return WB_MSG_DEBUG_TALK;
    if (strcmp(type_str, "wb_mic_gain") == 0)     return WB_MSG_MIC_GAIN;
    if (strcmp(type_str, "wb_key_event") == 0)    return WB_MSG_KEY_EVENT;
    if (strcmp(type_str, "wb_key_acked") == 0)    return WB_MSG_KEY_ACKED;
    if (strcmp(type_str, "wb_clear_screen") == 0) return WB_MSG_CLEAR_SCREEN;
    if (strcmp(type_str, "wb_reply") == 0)        return WB_MSG_REPLY;
    if (strcmp(type_str, "wb_tts_begin") == 0)    return WB_MSG_TTS_BEGIN;
    if (strcmp(type_str, "wb_tts_chunk") == 0)    return WB_MSG_TTS_CHUNK;
    if (strcmp(type_str, "wb_tts_end") == 0)      return WB_MSG_TTS_END;
    if (strcmp(type_str, "wb_tts_stop") == 0)     return WB_MSG_TTS_STOP;
    if (strcmp(type_str, "wb_header") == 0)       return WB_MSG_HEADER;
    return WB_MSG_UNKNOWN;
}

// 从 JSON payload 中取字符串字段（简化实现，不依赖 cJSON）
static bool json_get_str(const char *json, const char *key, char *out, size_t out_size) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);

    const char *pos = strstr(json, pattern);
    if (!pos) {
        // 尝试不带引号: "key":
        snprintf(pattern, sizeof(pattern), "\"%s\"", key);
        pos = strstr(json, pattern);
        if (!pos) return false;
        pos = strchr(pos, ':');
        if (!pos) return false;
        pos++; // 跳过冒号
    } else {
        pos += strlen(pattern);
    }

    // 跳过空白
    while (*pos == ' ' || *pos == '\t') pos++;

    if (*pos == '"') {
        // 字符串值
        pos++;
        const char *end = strchr(pos, '"');
        if (!end) return false;
        size_t len = (size_t)(end - pos);
        if (len >= out_size) len = out_size - 1;
        memcpy(out, pos, len);
        out[len] = '\0';
        return true;
    } else if (*pos == '{' || *pos == '[') {
        // 对象/数组，跳过到配对
        return false;
    } else {
        // 字面量
        const char *end = pos;
        while (*end && *end != ',' && *end != '}' && *end != '\n') end++;
        size_t len = (size_t)(end - pos);
        if (len >= out_size) len = out_size - 1;
        memcpy(out, pos, len);
        out[len] = '\0';
        return true;
    }
}

// 从 JSON payload 中取整数字段（缺省返回 def）
static int json_get_int(const char *json, const char *key, int def) {
    char v[24];
    if (!json_get_str(json, key, v, sizeof(v))) return def;
    char *end = NULL;
    long n = strtol(v, &end, 10);
    return (end && end != v) ? (int)n : def;
}

// 标准 base64 解码（容忍 '=' 填充；非法字符直接截断）
static size_t b64_decode(const char *src, uint8_t *out, size_t outcap) {
    size_t o = 0;
    uint32_t acc = 0;
    int nbits = 0;
    for (const char *p = src; *p; p++) {
        int c = (unsigned char)*p;
        int v;
        if (c >= 'A' && c <= 'Z') v = c - 'A';
        else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
        else if (c >= '0' && c <= '9') v = c - '0' + 52;
        else if (c == '+') v = 62;
        else if (c == '/') v = 63;
        else if (c == '=') break;
        else continue;              // 跳过换行/空白等
        acc = (acc << 6) | (uint32_t)v;
        nbits += 6;
        if (nbits >= 8) {
            nbits -= 8;
            if (o < outcap) out[o++] = (uint8_t)((acc >> nbits) & 0xFF);
            else break;
        }
    }
    return o;
}

// ============================================================
// 内部: 发送 JSON 消息
// ============================================================

esp_err_t wb_send_json(wb_protocol_t *p, const char *json) {
    if (!p || !p->connected) return ESP_ERR_INVALID_STATE;
    // 由调用方提供 GattNotify，实际发送由 BLE 层处理
    (void)json;
    return ESP_OK;
}

// ============================================================
// 公共 API
// ============================================================

wb_protocol_t *wb_protocol_new(void) {
    wb_protocol_t *p = calloc(1, sizeof(wb_protocol_t));
    if (p) {
        p->connected = false;
        p->conn_handle = 0;
    }
    return p;
}

void wb_protocol_free(wb_protocol_t *p) {
    if (p) free(p);
}

void wb_protocol_set_user_data(wb_protocol_t *p, void *data) {
    if (p) p->user_data = data;
}

void *wb_protocol_get_user_data(wb_protocol_t *p) {
    return p ? p->user_data : NULL;
}

bool wb_protocol_can_send(wb_protocol_t *p) {
    return p && p->connected;
}

// ============================================================
// 内部: 分包发送
// ============================================================

// GattNotify 回调类型（由 BLE 层注入）
static wb_gatt_notify_fn s_gatt_notify = NULL;

void wb_protocol_set_gatt_notify_cb(wb_gatt_notify_fn cb) {
    s_gatt_notify = cb;
}

static esp_err_t gatt_notify_str(wb_protocol_t *p, const char *str) {
    if (!p || !s_gatt_notify) return ESP_ERR_INVALID_STATE;
    size_t len = strlen(str);
    if (len == 0) return ESP_OK;

    // 整包发送（BLE 层会自动分包）
    return s_gatt_notify(p->conn_handle, (const uint8_t *)str, len);
}

// ============================================================
// 发送消息 (Device → PC)
// ============================================================

static char s_tmp_buf[512];

esp_err_t wb_send_voice_text(wb_protocol_t *p, const char *text) {
    if (!p || !text) return ESP_ERR_INVALID_ARG;
    snprintf(s_tmp_buf, sizeof(s_tmp_buf),
             "{\"type\":\"wb_voice_text\",\"payload\":{\"text\":\"%s\"}}\n", text);
    return gatt_notify_str(p, s_tmp_buf);
}

esp_err_t wb_send_option_selected(wb_protocol_t *p, const char *option_id) {
    if (!p || !option_id) return ESP_ERR_INVALID_ARG;
    snprintf(s_tmp_buf, sizeof(s_tmp_buf),
             "{\"type\":\"wb_option_selected\",\"payload\":{\"option_id\":\"%s\"}}\n", option_id);
    return gatt_notify_str(p, s_tmp_buf);
}

esp_err_t wb_send_notification(wb_protocol_t *p, const char *title, const char *body, const char *priority) {
    if (!p) return ESP_ERR_INVALID_ARG;
    snprintf(s_tmp_buf, sizeof(s_tmp_buf),
             "{\"type\":\"wb_notification\",\"payload\":{"
             "\"title\":\"%s\",\"body\":\"%s\",\"priority\":\"%s\"}}\n",
             title ? title : "", body ? body : "", priority ? priority : "normal");
    return gatt_notify_str(p, s_tmp_buf);
}

esp_err_t wb_send_task_complete(wb_protocol_t *p, const char *task_name, const char *result) {
    if (!p || !task_name) return ESP_ERR_INVALID_ARG;
    snprintf(s_tmp_buf, sizeof(s_tmp_buf),
             "{\"type\":\"wb_task_complete\",\"payload\":{"
             "\"task_name\":\"%s\",\"result\":\"%s\"}}\n",
             task_name, result ? result : "");
    return gatt_notify_str(p, s_tmp_buf);
}

esp_err_t wb_send_task_error(wb_protocol_t *p, const char *task_name, const char *error) {
    if (!p || !task_name) return ESP_ERR_INVALID_ARG;
    snprintf(s_tmp_buf, sizeof(s_tmp_buf),
             "{\"type\":\"wb_task_error\",\"payload\":{"
             "\"task_name\":\"%s\",\"error\":\"%s\"}}\n",
             task_name, error ? error : "");
    return gatt_notify_str(p, s_tmp_buf);
}

esp_err_t wb_send_inject_text(wb_protocol_t *p, const char *text, bool success) {
    if (!p || !text) return ESP_ERR_INVALID_ARG;
    snprintf(s_tmp_buf, sizeof(s_tmp_buf),
             "{\"type\":\"wb_inject_text\",\"payload\":{"
             "\"text\":\"%s\",\"success\":%s}}\n",
             text, success ? "true" : "false");
    return gatt_notify_str(p, s_tmp_buf);
}

// P3: 发送按键事件 (Device → PC)
esp_err_t wb_send_key_event(wb_protocol_t *p, const char *button, const char *event) {
    if (!p || !button || !event) return ESP_ERR_INVALID_ARG;
    snprintf(s_tmp_buf, sizeof(s_tmp_buf),
             "{\"type\":\"wb_key_event\",\"payload\":{\"button\":\"%s\",\"event\":\"%s\"}}\n",
             button, event);
    return gatt_notify_str(p, s_tmp_buf);
}

// 发送审批结果 (Device → PC)
esp_err_t wb_send_answer(wb_protocol_t *p, const char *id, const char *value) {
    if (!p || !value) return ESP_ERR_INVALID_ARG;
    snprintf(s_tmp_buf, sizeof(s_tmp_buf),
             "{\"type\":\"wb_answer\",\"payload\":{\"id\":\"%s\",\"v\":\"%s\"}}\n",
             id ? id : "", value);
    return gatt_notify_str(p, s_tmp_buf);
}

// 发送链路响应 (Device → PC)
esp_err_t wb_send_pong(wb_protocol_t *p) {
    if (!p) return ESP_ERR_INVALID_ARG;
    // 带上固件标识：PC 端据此确认连的是哪一版固件，避免"刷了旧镜像却以为在调新代码"
    snprintf(s_tmp_buf, sizeof(s_tmp_buf),
             "{\"type\":\"wb_pong\",\"payload\":{\"fw\":\"%s\",\"proto\":%d}}\n",
             WB_FW_VERSION, WB_PROTO_VERSION);
    return gatt_notify_str(p, s_tmp_buf);
}

// ============================================================
// 连接状态管理
// ============================================================

void wb_protocol_on_conn_change(wb_protocol_t *p, bool connected, uint16_t conn_handle) {
    if (!p) return;
    bool old = p->connected;
    p->connected = connected;
    p->conn_handle = conn_handle;

    if (connected && !old) {
        ESP_LOGI(TAG, "GATT 连接建立: conn_handle=%d", conn_handle);
    } else if (!connected && old) {
        ESP_LOGI(TAG, "GATT 连接断开");
    }

    if (p->on_conn_state) {
        p->on_conn_state(p, connected, conn_handle);
    }
}

// ============================================================
// 协议解析
// ============================================================

// 提取 JSON type 字段
static const char *extract_type(const char *json) {
    static char type_buf[64];
    const char *p = strstr(json, "\"type\"");
    if (!p) return NULL;
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '"') return NULL;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return NULL;
    size_t len = (size_t)(end - p);
    if (len >= sizeof(type_buf)) len = sizeof(type_buf) - 1;
    memcpy(type_buf, p, len);
    type_buf[len] = '\0';
    return type_buf;
}

// 提取 payload 对象（花括号配对）
static char *extract_payload(const char *json, char *out, size_t out_size) {
    const char *p = strstr(json, "\"payload\"");
    if (!p) { out[0] = '\0'; return out; }
    p = strchr(p, ':');
    if (!p) { out[0] = '\0'; return out; }
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '{') { out[0] = '\0'; return out; }

    int depth = 0;
    const char *start = p;
    const char *end = p;
    while (*end) {
        if (*end == '{') depth++;
        else if (*end == '}') {
            depth--;
            if (depth == 0) { end++; break; }
        }
        end++;
    }
    size_t len = (size_t)(end - start);
    if (len >= out_size) len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

// 解析并分发消息
static void dispatch_message(wb_protocol_t *p, const char *raw_json, size_t raw_len) {
    if (!p) return;

    // 任何成功解析到的消息都视为"PC 仍在线"（链路活性），刷新最后可见时间
    if (p->on_heartbeat) p->on_heartbeat(p);

    // 复制一份，末尾清零（放静态区：单行可达 2KB，压栈会撑爆 RX 任务栈）
    static char json[WB_RX_LINE_MAX];
    size_t copy_len = raw_len < sizeof(json) - 1 ? raw_len : sizeof(json) - 1;
    memcpy(json, raw_json, copy_len);
    json[copy_len] = '\0';

    const char *type_str = extract_type(json);
    if (!type_str) {
        ESP_LOGW(TAG, "无法解析消息 type 字段: %.*s", (int)raw_len, raw_json);
        return;
    }

    wb_msg_type_t type = parse_msg_type(type_str);
    static char payload[WB_RX_LINE_MAX];
    payload[0] = '\0';
    extract_payload(json, payload, sizeof(payload));

    ESP_LOGD(TAG, "收到消息 type=%s payload_len=%zu", type_str, strlen(payload));

    char val[320];

    switch (type) {
        case WB_MSG_SHOW_OPTIONS:
            ESP_LOGI(TAG, "收到 wb_show_options");
            if (p->on_show_options && payload[0]) {
                p->on_show_options(p, payload);
            }
            break;

        case WB_MSG_OPTION_SELECTED:
            if (json_get_str(payload, "option_id", val, sizeof(val)) && p->on_option_selected) {
                ESP_LOGI(TAG, "收到选项选中: %s", val);
                p->on_option_selected(p, val);
            }
            break;

        case WB_MSG_ASK:
            if (p->on_ask) {
                char id[32] = "", text[256] = "";
                json_get_str(payload, "id", id, sizeof(id));
                json_get_str(payload, "text", text, sizeof(text));
                ESP_LOGI(TAG, "收到 wb_ask id=%s", id);
                p->on_ask(p, id, text);
            }
            break;

        case WB_MSG_ANSWER:
            // Device → PC，设备不会收到
            break;

        case WB_MSG_NOTIFICATION:
            if (p->on_notification) {
                char title[128] = "", body[256] = "", priority[16] = "normal";
                json_get_str(payload, "title", title, sizeof(title));
                json_get_str(payload, "body", body, sizeof(body));
                json_get_str(payload, "priority", priority, sizeof(priority));
                p->on_notification(p, title, body, priority);
            }
            break;

        case WB_MSG_TASK_COMPLETE:
            if (p->on_notification) {
                char task_name[128] = "", result[256] = "";
                json_get_str(payload, "task_name", task_name, sizeof(task_name));
                json_get_str(payload, "result", result, sizeof(result));
                p->on_notification(p, "✅ 任务完成", task_name[0] ? task_name : result, "normal");
            }
            break;

        case WB_MSG_TASK_ERROR:
            if (p->on_notification) {
                char task_name[128] = "", error[256] = "";
                json_get_str(payload, "task_name", task_name, sizeof(task_name));
                json_get_str(payload, "error", error, sizeof(error));
                p->on_notification(p, "❌ 任务失败", task_name[0] ? task_name : error, "high");
            }
            break;

        case WB_MSG_STATUS: {
            char state[32] = "idle", text[256] = "";
            json_get_str(payload, "state", state, sizeof(state));
            json_get_str(payload, "text", text, sizeof(text));
            ESP_LOGI(TAG, "收到 wb_status state=%s", state);
            if (p->on_notification) {
                // 复用通知回调承载状态+文本（设备端据此更新状态屏与记录）
                p->on_notification(p, state, text, "status");
            }
            break;
        }

        case WB_MSG_PING:
            ESP_LOGI(TAG, "收到 wb_ping → pong");
            wb_send_pong(p);
            break;

        case WB_MSG_PONG:
            ESP_LOGI(TAG, "收到 wb_pong");
            break;

        case WB_MSG_HEARTBEAT:
            // 活性已在 dispatch 入口刷新；此处无需额外动作
            break;

        case WB_MSG_AUDIO:
        case WB_MSG_AUDIO_END:
            // 设备 → PC 上行音频；设备本身不会收到（保留分支以防万一）
            break;

        case WB_MSG_MIC_GAIN: {
            char dbv[24] = "15";
            json_get_str(payload, "db", dbv, sizeof(dbv));
            float db = (float)atof(dbv);
            ESP_LOGI(TAG, "设置麦克风增益 %.1f dB", db);
            if (p->on_mic_gain) p->on_mic_gain(p, db);
            break;
        }

        case WB_MSG_DEBUG_TALK: {
            char onv[16] = "false";
            json_get_str(payload, "on", onv, sizeof(onv));
            bool on = (strcmp(onv, "true") == 0 || strcmp(onv, "1") == 0);
            ESP_LOGI(TAG, "wb_debug_talk on=%d", on);
            if (p->on_debug_talk) p->on_debug_talk(p, on);
            break;
        }

        case WB_MSG_KEY_EVENT:
            ESP_LOGD(TAG, "收到 wb_key_event (不期望从 PC 收到)");
            break;

        case WB_MSG_KEY_ACKED:
            if (p->on_key_acked) {
                char btn[16] = "", evt[16] = "";
                bool ok = false;
                json_get_str(payload, "button", btn, sizeof(btn));
                json_get_str(payload, "event", evt, sizeof(evt));
                if (json_get_str(payload, "success", val, sizeof(val))) {
                    ok = (strcmp(val, "true") == 0 || strcmp(val, "1") == 0);
                }
                ESP_LOGI(TAG, "按键 ACK: button=%s event=%s success=%d", btn, evt, ok);
                p->on_key_acked(p, btn, evt, ok);
            }
            break;

        case WB_MSG_CLEAR_SCREEN:
            ESP_LOGI(TAG, "收到 wb_clear_screen");
            if (p->on_clear_screen) {
                p->on_clear_screen(p);
            }
            break;

        case WB_MSG_REPLY:
            if (json_get_str(payload, "text", val, sizeof(val)) && p->on_reply) {
                ESP_LOGI(TAG, "收到助手回复: %.*s", (int)strlen(val), val);
                p->on_reply(p, val);
            }
            break;

        case WB_MSG_TTS_BEGIN: {
            int hz = json_get_int(payload, "hz", 8000);
            int a  = json_get_int(payload, "a", 1);
            ESP_LOGI(TAG, "播报开始 hz=%d adpcm=%d", hz, a);
            if (p->on_tts_begin) p->on_tts_begin(p, hz, a != 0);
            break;
        }

        case WB_MSG_TTS_CHUNK: {
            static char b64buf[1600];
            static uint8_t raw[1200];
            int seq = json_get_int(payload, "s", 0);
            if (json_get_str(payload, "b", b64buf, sizeof(b64buf))) {
                size_t n = b64_decode(b64buf, raw, sizeof(raw));
                if (n > 0 && p->on_tts_chunk) p->on_tts_chunk(p, seq, raw, n);
            }
            break;
        }

        case WB_MSG_TTS_END:
        case WB_MSG_TTS_STOP:
            if (p->on_tts_end) p->on_tts_end(p);
            break;

        case WB_MSG_HEADER:
            if (json_get_str(payload, "text", val, sizeof(val)) && p->on_header) {
                p->on_header(p, val);
            }
            break;

        default:
            ESP_LOGD(TAG, "未知消息类型: %s", type_str);
            break;
    }
}

// 接收到的原始数据缓冲（音频上行行可能较长，留足余量）
static char s_line_buf[4096];
static size_t s_line_len = 0;

void wb_protocol_on_rx_data(wb_protocol_t *p, const uint8_t *data, size_t len) {
    if (!p || !data || len == 0) return;

    for (size_t i = 0; i < len; i++) {
        char c = (char)data[i];

        if (c == '\n' || c == '\r') {
            // 行结束
            if (s_line_len > 0) {
                s_line_buf[s_line_len] = '\0';
                dispatch_message(p, s_line_buf, s_line_len);
                s_line_len = 0;
            }
            // 忽略换行符本身
            continue;
        }

        if (s_line_len < sizeof(s_line_buf) - 1) {
            s_line_buf[s_line_len++] = c;
        } else {
            // 缓冲满，截断
            ESP_LOGW(TAG, "协议缓冲溢出，清空");
            s_line_len = 0;
        }
    }
}
