// main/finder_scan.h —— NimBLE 观察者（被动扫描）封装。
//
// 与 demo_ble.c 的区别：那里是广播（peripheral/broadcaster），这里是扫描（observer）。
// 协议栈的初始化/停止/反初始化顺序完全复用 demo_ble.c 的骨架，包括 host 任务的
// 创建、停止握手与清理重试——直接照抄，不要从回调里调 nimble_port_deinit。
//
// 自身广播：本应用【不广播】。因此 PRD §12 里"100% 扫描会挤掉设备自身广播"这一条
// 在本应用不适用；这里把占空比压到 30–50% 纯粹是为了【功耗】，不是为了保住广播。
//
// 回调纪律：cb 运行在 NimBLE host 任务内，只能入队或做同等级的有界操作。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define FINDER_SCAN_NAME_MAX 32 // 单条广播名上限（BLE 广播名实际不超过 29 字节）

// 占空比：window / interval 必须同时小于 interval。30–50% 是功耗与响应性的折中。
// 更新率由【被扫描设备】的广播间隔决定，不由 window 决定，所以降低占空比不损功能。
#define FINDER_SCAN_INTERVAL_MS 100
#define FINDER_SCAN_WINDOW_MS    40

typedef struct {
    uint8_t addr[6];
    int32_t rssi;
    bool    has_name;
    char    name[FINDER_SCAN_NAME_MAX + 1];
} finder_scan_result_t;

typedef void (*finder_scan_cb_t)(const finder_scan_result_t *result, void *user);

// 启动被动扫描。可重复调用失败后重试；已启动时返回 ESP_ERR_INVALID_STATE。
esp_err_t finder_scan_start(finder_scan_cb_t cb, void *user);

// 停止扫描并反初始化协议栈。返回非 ESP_OK 表示清理未完成，调用方应保留页面并提示，
// 不要静默忽略——否则下次进入会撞上"协议栈已在运行"。
esp_err_t finder_scan_stop(void);

bool finder_scan_running(void);

// 最近一次失败的原因（NimBLE 返回码或 esp_err_t），供界面显示与诊断。
int finder_scan_last_error(void);
