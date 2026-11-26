// main/prov_form.c —— 配网表单解析:不建 JSON/不依赖 ESP-IDF,纯字符串处理。
// 单独成文件是为了能在主机上跑测试(网页表单的边界条件比联网本身更容易出错)。
// 校验提示只出现在手机网页上,不占设备字形集:glyph-check: ignore-file
#include "prov_form.h"

#include <string.h>

#define SSID_MAX_LEN 32
#define PASS_MIN_LEN 8
#define PASS_MAX_LEN 63

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// URL 解码:'+' 还原成空格,%XX 还原成字节。缓冲区不够就整体失败,不截断。
static bool decode_value(const char *src, size_t len, char *dst, size_t cap) {
    if (cap == 0) return false;
    size_t out = 0;
    for (size_t i = 0; i < len; i++) {
        char c = src[i];
        if (c == '+') {
            c = ' ';
        } else if (c == '%') {
            if (i + 2 >= len) return false;
            int hi = hex_val(src[i + 1]);
            int lo = hex_val(src[i + 2]);
            if (hi < 0 || lo < 0) return false;
            c = (char)((hi << 4) | lo);
            i += 2;
        }
        if (out + 1 >= cap) return false;
        dst[out++] = c;
    }
    dst[out] = '\0';
    return true;
}

bool prov_form_parse(const char *body, size_t len,
                     char *ssid, size_t ssid_cap,
                     char *pass, size_t pass_cap) {
    if (body == NULL || ssid == NULL || ssid_cap == 0) return false;
    ssid[0] = '\0';
    if (pass != NULL && pass_cap > 0) pass[0] = '\0';

    bool found_ssid = false;
    size_t i = 0;
    while (i <= len) {
        // 切出 key=value,以 '&' 或结尾为界。
        size_t start = i;
        while (i < len && body[i] != '&') i++;
        size_t field_len = i - start;
        if (field_len > 0) {
            size_t eq = start;
            while (eq < start + field_len && body[eq] != '=') eq++;
            size_t key_len = eq - start;
            const char *val = body + eq + 1;
            size_t val_len = (eq < start + field_len) ? (start + field_len - eq - 1) : 0;

            if (key_len == 4 && memcmp(body + start, "ssid", 4) == 0) {
                if (!decode_value(val, val_len, ssid, ssid_cap)) return false;
                found_ssid = true;
            } else if (key_len == 4 && memcmp(body + start, "pass", 4) == 0 && pass != NULL && pass_cap > 0) {
                if (!decode_value(val, val_len, pass, pass_cap)) return false;
            }
        }
        i++;   // 跳过 '&'
    }
    return found_ssid;
}

prov_form_err_t prov_form_check(const char *ssid, const char *pass) {
    size_t sl = (ssid != NULL) ? strlen(ssid) : 0;
    size_t pl = (pass != NULL) ? strlen(pass) : 0;
    if (sl == 0) return PROV_FORM_ERR_SSID_EMPTY;
    if (sl > SSID_MAX_LEN) return PROV_FORM_ERR_SSID_LONG;
    if (pl > PASS_MAX_LEN) return PROV_FORM_ERR_PASS_LONG;
    // 空密码当开放网络;1~7 位既不是合法 WPA2 也不像开放网络,直接判错。
    if (pl > 0 && pl < PASS_MIN_LEN) return PROV_FORM_ERR_PASS_SHORT;
    return PROV_FORM_OK;
}

const char *prov_form_error_text(prov_form_err_t err) {
    switch (err) {
        case PROV_FORM_ERR_SSID_EMPTY: return "WiFi 名称不能为空";
        case PROV_FORM_ERR_SSID_LONG:  return "WiFi 名称超过 32 字节";
        case PROV_FORM_ERR_PASS_LONG:  return "密码超过 63 位";
        case PROV_FORM_ERR_PASS_SHORT: return "密码至少 8 位(开放网络请留空)";
        case PROV_FORM_ERR_ENCODING:   return "表单编码异常,请重新输入";
        default:                       return "";
    }
}
