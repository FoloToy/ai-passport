// main/net_wifi.h —— WiFi STA 联网:开机自动连接,断线自动重连。
// 凭据运行时从 NVS 读取(手机配网写入),换网络不需要重编固件,固件内不含密码。
#pragma once

#include <stdbool.h>

// 初始化网络栈并开始连接(非阻塞)。没有任何凭据时不启动 STA,
// 但仍会准备好 netif/事件循环/WiFi 驱动,便于配网模块直接起 SoftAP。
void net_wifi_start(void);

// 网络栈是否初始化成功(配网模块据此判断能否直接起 AP)。
bool net_wifi_stack_ready(void);

// 是否有可用凭据(NVS 或编译期默认值)。
bool net_wifi_configured(void);

// 当前尝试连接的网络名;没有凭据时返回空串。
const char *net_wifi_ssid(void);

// 当前是否已拿到 IP。
bool net_wifi_connected(void);
