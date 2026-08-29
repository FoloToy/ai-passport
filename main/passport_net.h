// main/passport_net.h —— 网络任务：Wi-Fi 突发连接、SNTP 校时、HTTPS API 客户端。
//
// 对齐上游"进入时初始化、退出时释放"模式：每次同步/心跳是一次完整突发
// （连 Wi-Fi → 校时 → auth → 业务调用 → esp_wifi_stop），Wi-Fi 不常开。
// 本任务不直接操作任何 LVGL 对象；结果经 ui_queue 投递给 UI 上下文。
#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    PASSPORT_NET_REQ_SYNC = 1,   // 开机/积分页/兜底：auth → /passport/me → history
    PASSPORT_NET_REQ_CHECKIN,    // 每日签到
    PASSPORT_NET_REQ_HEARTBEAT,  // 在馆心跳（仅校区 SSID，见实现文档 7.5 节）
} passport_net_req_t;

typedef struct {
    int32_t delta;               // 积分变动（+20 / -100）
    char    text[24];            // 变动说明（到课/兑换…）
} passport_point_item_t;

#define PASSPORT_HISTORY_MAX 3

// 创建网络任务。ui_queue 元素类型为 passport_ui_evt_t（见 passport_ui.h）。
void passport_net_init(QueueHandle_t ui_queue);

// 投递一次请求（队列缓冲，突发中合并）。
void passport_net_request(passport_net_req_t req);

// 是否正处于突发（联网/同步/心跳中）
bool passport_net_is_busy(void);

// 最近一次同步的积分流水（网络任务写、UI 读，内部有锁）
int passport_net_history(passport_point_item_t out[PASSPORT_HISTORY_MAX]);
