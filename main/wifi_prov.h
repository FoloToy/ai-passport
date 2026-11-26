// main/wifi_prov.h —— 手机网页配网:设备开热点,手机连上后填 SSID/密码。
// 面向「不固定 WiFi」的场景:搬到别处只需要用手机重新配一次,不用重编固件。
#pragma once

#include <stdbool.h>

#include "esp_err.h"

typedef enum {
    WIFI_PROV_IDLE = 0,   // 未开启
    WIFI_PROV_WAITING,    // 热点已开,等手机提交
    WIFI_PROV_SAVED,      // 已保存,马上重启
} wifi_prov_state_t;

// 开启热点 + 网页服务(APSTA:STA 侧连着的网络不会断,便于用电脑在同网段验证)。
esp_err_t wifi_prov_start(void);

// 关闭网页服务,WiFi 切回纯 STA 模式。
void wifi_prov_stop(void);

bool wifi_prov_active(void);

// 热点名,形如 FoloToy-7F28(取 STA MAC 后两字节)。
const char *wifi_prov_ap_ssid(void);

wifi_prov_state_t wifi_prov_state(void);
