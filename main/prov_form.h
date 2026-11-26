// main/prov_form.h —— 配网表单的纯解析/校验(不依赖 ESP-IDF,可主机测试)。
// 网页提交的是 application/x-www-form-urlencoded: ssid=xxx&pass=yyy
#pragma once

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    PROV_FORM_OK = 0,
    PROV_FORM_ERR_SSID_EMPTY,
    PROV_FORM_ERR_SSID_LONG,
    PROV_FORM_ERR_PASS_LONG,
    PROV_FORM_ERR_PASS_SHORT,
    PROV_FORM_ERR_ENCODING,
} prov_form_err_t;

// 解析表单正文。找到 ssid 字段即返回 true(密码缺省为空,表示开放网络)。
// 字符串按 URL 编码解码(%XX 与 '+'),解码后超长或编码非法返回 false。
bool prov_form_parse(const char *body, size_t len,
                     char *ssid, size_t ssid_cap,
                     char *pass, size_t pass_cap);

// 校验长度是否是可用的 WPA2 凭据。密码为空视为开放网络,允许通过。
prov_form_err_t prov_form_check(const char *ssid, const char *pass);

// 校验失败的中文说明(直接显示在网页上)。
const char *prov_form_error_text(prov_form_err_t err);
