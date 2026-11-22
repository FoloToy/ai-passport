// components/treaai/include/treaai_protocol.h
// JSON 协议编解码 — 纯函数、无 ESP-IDF 依赖, 方便 host test。
#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// 构建 POST JSON: { "agent_id": "...", "text": "用户问..." }
// 返回堆分配字符串 (caller free)
char *treaai_build_text_request(const char *agent_id, const char *text);

// 构建 POST JSON: 带 base64 音频 (长度上限 TREAAI_MAX_TEXT_LEN 级别)
char *treaai_build_audio_request(const char *agent_id,
                                 const char *b64_audio, size_t b64_len);

// 解析 AI 响应 JSON: 提取 "reply" 文本和可选的 "audio_base64"
// text_out: 调用者提供 buffer, 必须 >= TREAAI_MAX_RESPONSE_LEN
// audio_out: 调用者提供 buffer, 返回 base64 长度; NULL=只取文本
// 成功返回 ESP_OK; reply 字段缺失返回 ESP_ERR_NOT_FOUND
esp_err_t treaai_parse_response(const char *json,
                                char *text_out, size_t text_size,
                                char *audio_out, size_t audio_size,
                                size_t *audio_len_out);

// host 测试辅助: 对 const char * 做 URL-safe base64 编码的解码 (mbedtls 自带)
// 这里只声明 API, 实现见 treaai_protocol.c
int treaai_b64_decode(const char *in, size_t in_len,
                      uint8_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif
