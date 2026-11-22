// components/treaai/src/treaai_protocol.c
// JSON 协议编解码 — 纯 cJSON 调用 + mbedtls base64。
#include "treaai_protocol.h"
#include "esp_log.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "treaai_proto";

char *treaai_build_text_request(const char *agent_id, const char *text)
{
    if (!text || !agent_id) return NULL;
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "agent_id", agent_id);
    cJSON_AddStringToObject(root, "message", text);
    cJSON *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

char *treaai_build_audio_request(const char *agent_id,
                                 const char *b64_audio, size_t b64_len)
{
    if (!agent_id || !b64_audio) return NULL;
    cJSON *root = cJSON_CreateObject();
    if (!root) return NULL;
    cJSON_AddStringToObject(root, "agent_id", agent_id);
    cJSON_AddStringToObject(root, "audio_base64", b64_audio);
    cJSON_AddNumberToObject(root, "audio_format", 16000);
    cJSON *out = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return out;
}

esp_err_t treaai_parse_response(const char *json,
                                char *text_out, size_t text_size,
                                char *audio_out, size_t audio_size,
                                size_t *audio_len_out)
{
    if (!json || !text_out || text_size == 0) return ESP_ERR_INVALID_ARG;

    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGW(TAG, "JSON parse failed");
        return ESP_ERR_INVALID_RESPONSE_FORMAT;
    }

    // 优先走 data.reply, 兼容顶层 reply
    cJSON *reply = cJSON_GetObjectItemCaseSensitive(root, "reply");
    if (!reply) {
        cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
        if (data) reply = cJSON_GetObjectItemCaseSensitive(data, "reply");
    }
    if (!reply || !cJSON_IsString(reply)) {
        cJSON_Delete(root);
        return ESP_ERR_NOT_FOUND;
    }
    strncpy(text_out, reply->valuestring, text_size - 1);
    text_out[text_size - 1] = '\0';

    if (audio_len_out) *audio_len_out = 0;
    if (audio_out && audio_size > 0) {
        cJSON *audio = cJSON_GetObjectItemCaseSensitive(root, "audio_base64");
        if (!audio) {
            cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
            if (data) audio = cJSON_GetObjectItemCaseSensitive(data, "audio_base64");
        }
        if (audio && cJSON_IsString(audio)) {
            size_t dlen = 0;
            mbedtls_base64_decode((unsigned char *)audio_out, audio_size, &dlen,
                                  (const unsigned char *)audio->valuestring,
                                  strlen(audio->valuestring));
            if (audio_len_out) *audio_len_out = dlen;
        }
    }

    cJSON_Delete(root);
    return ESP_OK;
}

int treaai_b64_decode(const char *in, size_t in_len,
                      uint8_t *out, size_t out_cap)
{
    size_t dlen = 0;
    int r = mbedtls_base64_decode(out, out_cap, &dlen,
                                  (const unsigned char *)in, in_len);
    if (r != 0) return -1;
    return (int)dlen;
}
