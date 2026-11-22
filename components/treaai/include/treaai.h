// components/treaai/include/treaai.h
// TreaAI Passport — AI Agent 客户端公共 API。
// ESP32-C3 轻量实现:HTTP + JSON, 语音走 base64 PCM 上传。
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---------------------------------------------------------------------------
// 配置
// ---------------------------------------------------------------------------

// NVS 命名空间
#define TREAAI_NVS_NS       "treaai"

// API 默认端点 (Trae AI 网关 — 可在设置里改)
#define TREAAI_DEFAULT_HOST "api.trae.ai"
#define TREAAI_DEFAULT_PORT 443
#define TREAAI_DEFAULT_PATH "/v1/agent/chat"

// 请求/响应尺寸上限
#define TREAAI_MAX_TEXT_LEN     512   // 单条文本上下限
#define TREAAI_MAX_RESPONSE_LEN 2048  // AI 回复上限
#define TREAAI_AUDIO_CHUNK_MS   20    // 录音上传分片 (ms)

// 预设快捷指令
#define TREAAI_QUICK_CMD_COUNT  6

typedef struct {
    const char *key;     // NVS key 标识
    const char *label;   // 屏幕显示
    const char *default_prompt;
} treaai_quick_cmd_t;

extern const treaai_quick_cmd_t treaai_quick_cmds[TREAAI_QUICK_CMD_COUNT];

// ---------------------------------------------------------------------------
// 状态
// ---------------------------------------------------------------------------

typedef enum {
    TREAAI_STATE_IDLE = 0,         // 空闲
    TREAAI_STATE_CONNECTING,       // 连接 AI 网关
    TREAAI_STATE_LISTENING,        // 录音中
    TREAAI_STATE_THINKING,         // 等待 AI 回复
    TREAAI_STATE_SPEAKING,         // 播放 AI 回复
    TREAAI_STATE_ERROR,            // 错误态 (看 last_error)
} treaai_state_t;

typedef struct {
    treaai_state_t state;
    int            http_code;      // 最近 HTTP 状态码, 0=没请求过
    char           last_error[64]; // 最近错误字符串
    int            battery_soc;    // 缓存电量百分比, -1=未知
    bool           wifi_connected;
    bool           server_reachable;
} treaai_status_t;

// 异步结果回调: 文本回复 or 错误
typedef void (*treaai_response_cb_t)(const char *text, int audio_len, void *user);

// ---------------------------------------------------------------------------
// 初始化 / 配置持久化
// ---------------------------------------------------------------------------

esp_err_t treaai_init(void);

// 读写配置 (NVS)
esp_err_t treaai_set_api_key(const char *key);
esp_err_t treaai_get_api_key(char *out, size_t out_size);
esp_err_t treaai_set_endpoint(const char *host, int port, const char *path);
esp_err_t treaai_get_endpoint(char *host, size_t hsize, int *port,
                              char *path, size_t psize);
esp_err_t treaai_set_quick_prompt(int idx, const char *prompt);
esp_err_t treaai_get_quick_prompt(int idx, char *out, size_t out_size);

// ---------------------------------------------------------------------------
// 状态查询
// ---------------------------------------------------------------------------

treaai_state_t treaai_get_state(void);
void          treaai_get_status(treaai_status_t *out);

// ---------------------------------------------------------------------------
// AI 交互
// ---------------------------------------------------------------------------

// 发送纯文本 query, 异步回调返回 AI 文本回复 + 可选音频长度
esp_err_t treaai_send_text(const char *text, treaai_response_cb_t cb, void *user);

// 上传录音 chunk (单声道 16bit 16kHz PCM), 自动累计到一次请求
esp_err_t treaai_upload_audio_chunk(const int16_t *pcm, size_t samples);

// 结束录音、发送请求、回调返回结果
esp_err_t treaai_send_audio(treaai_response_cb_t cb, void *user);

// 中止当前请求
void treaai_cancel(void);

// 测试 Wi-Fi 连接 + API 可达性
esp_err_t treaai_test_connectivity(bool *out_wifi_ok, bool *out_server_ok);

#ifdef __cplusplus
}
#endif
