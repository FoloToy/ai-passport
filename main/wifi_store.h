// main/wifi_store.h —— WiFi 凭据持久化(NVS)。
// 配网保存的 SSID/密码写进 NVS,换网络不用重新编译固件。
// 只存凭据本身,不存「上次连过谁」之类的运行态。
#pragma once

#include <stdbool.h>
#include <stddef.h>

// 读取已保存的凭据。返回 true 表示 NVS 里有非空 SSID。
// 密码可以为空(开放网络);未保存过时输出缓冲区置空串。
bool wifi_store_load(char *ssid, size_t ssid_cap, char *pass, size_t pass_cap);

// 保存凭据(覆盖旧值)。ssid 为空返回 false。
bool wifi_store_save(const char *ssid, const char *pass);

// 清空凭据,下次开机回到「未配置网络」。
bool wifi_store_clear(void);
