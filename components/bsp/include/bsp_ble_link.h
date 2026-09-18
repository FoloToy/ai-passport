// components/bsp/include/bsp_ble_link.h —— 两台设备之间的 BLE 点对点字节链路。
//
// 设计目标:让应用拿到的只是一条“能收发字节、断了会告诉你”的链路,不用关心
// GATT 句柄、角色和连接流程。做法:
//
//   * 对等发现:两台设备【同时】广播一个自定义服务,又同时扫描同名对端。
//     谁发起连接由 6 字节 BLE 地址大小决定(地址大的主动连),因此这一侧是
//     central、另一侧是 peripheral。两边算出同一个结果,不需要用户在界面上
//     选“主机/加入”,也不会出现两个人同时连对方。
//   * 只支持 1:1。`CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1`,多台设备同场时会按
//     同样的地址规则两两配对,当前不做仲裁,不要在一个场地放三台以上。
//   * 需要四个 NimBLE 角色:peripheral + broadcaster(广播自己)、central +
//     observer(扫描并主动连对端)。模板为给示例瘦身把 CENTRAL/OBSERVER 默认设成 n,
//     用本模块前必须在 sdkconfig 里打开这四个角色(否则连不上,且不会报错)。
//   * 服务里有一条可写特征(对端写进来)和一条可读+可通知特征(本机发出去)。
//     可读那一侧返回两个字节的识别信息,给手机/通用 BLE 工具调试用。
//   * 收发都是“尽力而为”的字节流:peripheral 侧用 notify、central 侧用
//     write-without-response,两者都没有 ATT 层确认。需要可靠投递的协议必须
//     自己在应用层做序号 + ACK(见 main/c4_link_proto.c)。
//   * 应用只需要关心状态机 IDLE → DISCOVERING → CONNECTING → READY,
//     READY 之后才能 send。
//
// 线程约定:状态回调与接收回调都运行在 NimBLE host 任务里,必须短小 —— 只允许
// 拷贝数据、入队或置标志;禁止阻塞、禁止访问 LVGL。
#pragma once

#include "esp_err.h"

#include <stdbool.h>
#include <stdint.h>

// 单次收发的最大字节数。默认 ATT MTU 是 23,应用层负载只有 20 字节;
// 这里留出余量给后续协商更大 MTU。
#define BSP_BLE_LINK_MAX_PAYLOAD 32

typedef enum {
    BSP_BLE_LINK_IDLE = 0,      // 未启动,或已 stop
    BSP_BLE_LINK_DISCOVERING,   // 正在广播 + 扫描,等对端出现
    BSP_BLE_LINK_CONNECTING,    // 已锁定对端(或已被连上),正在建立/发现 GATT
    BSP_BLE_LINK_READY,         // 双向通道就绪,可以 send
    BSP_BLE_LINK_FAILED,        // 控制器/host 启动失败,链路不可用
} bsp_ble_link_state_t;

// 状态变化通知(可能在任意一刻从 DISCOVERING 回到 DISCOVERING:对端断开后重新找)。
typedef void (*bsp_ble_link_state_cb_t)(bsp_ble_link_state_t state, void *user);

// 收到对端一帧数据。data 只在回调期间有效,需要保留请自行拷贝。
typedef void (*bsp_ble_link_rx_cb_t)(const uint8_t *data, uint16_t len, void *user);

typedef struct {
    // 本机广播名(对端扫描时看到的名字),如 "C4-1A2B"。
    const char *local_name;
    // 对端发现用的名字前缀。只有广播名以此开头的对端才会被连接,如 "C4-"。
    const char *name_prefix;
    // 128 位 UUID,按小端字节序存放(与 BLE_UUID128_INIT 的参数顺序一致)。
    // 服务 / 对端写入通道 / 本机通知通道。
    uint8_t service_uuid[16];
    uint8_t rx_uuid[16];
    uint8_t tx_uuid[16];
} bsp_ble_link_cfg_t;

// 启动链路:初始化 NimBLE(NimBLE 内存约 73KB 堆,无 PSRAM 的板子上要先腾出空间)、
// 注册 GATT 服务、开始广播 + 扫描。cfg 的内容会被拷走,调用后可以释放。
// 成功返回 ESP_OK;失败返回 ESP_FAIL/ESP_ERR_NO_MEM 等,失败后可重试。
esp_err_t bsp_ble_link_start(const bsp_ble_link_cfg_t *cfg,
                             bsp_ble_link_state_cb_t on_state,
                             bsp_ble_link_rx_cb_t on_rx,
                             void *user);

// 停链路:停广播/扫描、停 NimBLE host 并释放控制器。进入 deep sleep 前必须调用,
// 否则射频仍被供电。停止期间不再回调应用。可重复调用。
esp_err_t bsp_ble_link_stop(void);

// 发送一帧。只有 READY 状态才允许;未连接返回 ESP_ERR_INVALID_STATE。
// 无 ATT 层确认:失败或丢包由应用层序号/ACK 负责。
esp_err_t bsp_ble_link_send(const uint8_t *data, uint16_t len);

// 是否已经 READY(双向通道就绪)。
bool bsp_ble_link_ready(void);

// 本机在当前连接里是否是主动连接方(central)。central/peripheral 的区分只影响
// 谁先手之类的中立约定,由调用方自行决定怎么用。
bool bsp_ble_link_is_central(void);
