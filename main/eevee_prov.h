#pragma once

#include "eevee_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*eevee_prov_cb_t)(const eevee_config_t *new_cfg, bool success);

/**
 * 启动 SoftAP + Web 配网服务
 * @param current_cfg 当前内存中的配置指针 (用于默认值)
 * @param cb 配网结果回调 (成功或失败)
 */
bool eevee_prov_start(const eevee_config_t *current_cfg, eevee_prov_cb_t cb);

/**
 * 停止 SoftAP + Web 配网服务，释放 HTTP 服务与网络热点
 */
void eevee_prov_stop(void);

/**
 * 查询配网服务是否正在运行
 */
bool eevee_prov_is_running(void);

#ifdef __cplusplus
}
#endif
