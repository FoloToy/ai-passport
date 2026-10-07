#pragma once

#include "eevee_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 重置配置为默认值
 */
void eevee_config_reset_defaults(eevee_config_t *cfg);

/**
 * 初始化配置，从 NVS 读取，若未配置则加载合理默认值
 */
void eevee_config_init(eevee_config_t *cfg);

/**
 * 保存配置至 NVS
 */
bool eevee_config_save(const eevee_config_t *cfg);

#ifndef EEVEE_DEFAULT_BASE_URL
#define EEVEE_DEFAULT_BASE_URL "http://192.168.8.100:3001"
#endif

#ifndef EEVEE_DEFAULT_WIFI_SSID
#define EEVEE_DEFAULT_WIFI_SSID ""
#endif

#ifndef EEVEE_DEFAULT_WIFI_PASS
#define EEVEE_DEFAULT_WIFI_PASS ""
#endif

#ifndef EEVEE_DEFAULT_PAT_TOKEN
#define EEVEE_DEFAULT_PAT_TOKEN "usr_bIqR70McZHO4HWNP5veO1EStkatzpMXc"
#endif

#ifndef EEVEE_DEFAULT_APP_ID
#define EEVEE_DEFAULT_APP_ID "01a11567-c699-7c91-a114-6570983433c5"
#endif

/**
 * 判断当前配置是否具备有效的 Wi-Fi 凭据
 */
bool eevee_config_has_wifi(const eevee_config_t *cfg);

/**
 * 获取对应 Web 前端基地址 (将 API 后端 3001 端口映射至 Web 前端 3000 端口)
 */
void eevee_config_get_web_url(const eevee_config_t *cfg, char *out_buf, size_t max_len);

#ifdef __cplusplus
}
#endif
