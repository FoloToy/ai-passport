// components/bsp/src/bsp_ble_link.c —— 对等 BLE 字节链路的 NimBLE 实现。
//
// 角色与发现规则(两边对称,不需要用户选主从):
//   1. 启动后同时做两件事:广播自己的 128 位服务 + 主动扫描同名前缀的对端。
//   2. 扫到对端后比较 6 字节地址(按扫描器里显示的字节顺序,最高字节在前):
//      自己的地址大 → 由本机主动连(central);否则什么都不做,等对方连进来
//      (peripheral)。规则反对称,因此两边算出的结果一定相反,谁都不会重复发起。
//      比较本身在 bsp_ble_addr.c,由 tests/test_bsp_ble_addr.c 覆盖。
//   3. 连接建立后:central 侧发现对端服务 → 写 CCCD 订阅通知 → READY;
//      peripheral 侧等到对端订阅通知 → READY。两边都 READY 才允许 send。
//   4. 断开后清状态、重新进入 DISCOVERING,并通知应用。
//
// 只有 1 个连接(CONFIG_BT_NIMBLE_MAX_CONNECTIONS=1),只做 1:1。
#include "bsp_ble_link.h"

#include "bsp_ble_addr.h"

#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_hs_id.h"
#include "host/ble_hs_mbuf.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "nvs_flash.h"
#include "os/os_mbuf.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

#include <string.h>

static const char *TAG = "bsp_ble";

// 主动连接的超时(对端可能刚好在重启/走远)。超时后由 host 报连接失败,
// gap_event 收到后重新回到 DISCOVERING。
#define BSP_BLE_CONNECT_TIMEOUT_MS 10000
// 停止 host 的等待上限,与 demo_ble 的既有做法一致。
#define BSP_BLE_STOP_TIMEOUT_MS    2000

// 连接参数:回合制通信不需要低延迟,拉长间隔省电。itvl 单位 1.25ms,
// supervision timeout 单位 10ms。
static const struct ble_gap_conn_params s_conn_params = {
    .scan_itvl = 0x0010,
    .scan_window = 0x0010,
    .itvl_min = 0x0018,             // 30ms
    .itvl_max = 0x0028,             // 50ms
    .latency = 0,
    .supervision_timeout = 0x01F4,  // 5s
    .min_ce_len = 0,
    .max_ce_len = 0,
};

// 名字在广播和发现里反复用到,拷进静态缓冲,调用方无需保证生命周期。
#define BSP_BLE_NAME_MAX   16
#define BSP_BLE_PREFIX_MAX  8

static char s_local_name[BSP_BLE_NAME_MAX];
static char s_name_prefix[BSP_BLE_PREFIX_MAX];
static ble_uuid128_t s_svc_uuid;
static ble_uuid128_t s_rx_uuid;
static ble_uuid128_t s_tx_uuid;

static bsp_ble_link_state_cb_t s_on_state;
static bsp_ble_link_rx_cb_t s_on_rx;
static void *s_user;

static SemaphoreHandle_t s_host_stopped;
static TaskHandle_t s_host_task;
static bool s_started;
static bool s_host_done;
static bool s_stop_in_progress;
// 停机进行中:所有 GAP 回调短路,不再重启广播/扫描,也不再回调应用。
// 没有这个标志时,stop() 期间由 ble_hs_stop() 终止连接所产生的 DISCONNECT 会走到
// on_disconnect() → discovery_start(),在 host 已经关闭之后再去开广播/扫描(实测只会
// 打出两条 BLE_HS_EDISABLED 假报错,但正确性不该依赖这个回调时序)。
static bool s_stopping;

static uint8_t s_addr_type;
static ble_addr_t s_own_addr;
static bool s_have_own_addr;
static uint8_t s_state = BSP_BLE_LINK_IDLE;

static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE;
static bool s_connecting;              // 本机正在主动连接(用于区分 central/peripheral)
static bool s_is_central;

// peripheral 侧:本机 TX 特征值句柄(notify 用)与对端是否已订阅。
static uint16_t s_own_tx_val;
static bool s_peer_subscribed;

// central 侧:对端服务发现结果。
static uint16_t s_peer_rx_val;         // 对端写入通道(我们写它)
static uint16_t s_peer_tx_val;         // 对端通知通道(我们订阅它)
static uint16_t s_peer_svc_start;
static uint16_t s_peer_svc_end;
static uint16_t s_peer_tx_next_def;    // TX 之后第一个特征的定义句柄,0 表示没有
static bool s_peer_rx_found;
static bool s_cccd_found;              // 是否已在本轮发现里找到可订阅的描述符

static int gap_event(struct ble_gap_event *event, void *arg);

static void set_state(uint8_t state)
{
    if (s_state == state) return;
    s_state = state;
    if (s_on_state) s_on_state((bsp_ble_link_state_t)state, s_user);
}

static void link_reset(void)
{
    s_conn = BLE_HS_CONN_HANDLE_NONE;
    s_connecting = false;
    s_is_central = false;
    s_peer_subscribed = false;
    s_peer_rx_val = 0;
    s_peer_tx_val = 0;
    s_peer_rx_found = false;
    s_cccd_found = false;
    s_peer_svc_start = 0;
    s_peer_svc_end = 0;
    s_peer_tx_next_def = 0;
}

// ——— 广播 / 扫描 ———————————————————————————————————————————————

static int advertise(void)
{
    // 广播里放 flags + 完整设备名:对端靠名字前缀发现彼此,不依赖 UUID,
    // 这样 31 字节的广播包一定装得下(名字最长按 8 字符设计)。
    struct ble_hs_adv_fields fields = { 0 };
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)s_local_name;
    fields.name_len = (uint8_t)strlen(s_local_name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "广播字段设置失败 rc=%d", rc);
        return rc;
    }

    // 128 位服务 UUID 放进扫描响应,给手机/通用 BLE 工具看,不参与发现。
    struct ble_hs_adv_fields rsp = { 0 };
    rsp.uuids128 = &s_svc_uuid;
    rsp.num_uuids128 = 1;
    rsp.uuids128_is_complete = 1;
    (void)ble_gap_adv_rsp_set_fields(&rsp);

    struct ble_gap_adv_params params = { 0 };
    params.conn_mode = BLE_GAP_CONN_MODE_UND;   // 可连接:对端要能连进来
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) ESP_LOGE(TAG, "广播启动失败 rc=%d", rc);
    return rc;
}

static int scan_start(void)
{
    struct ble_gap_disc_params params = { 0 };
    params.itvl = 0;                      // 0 = 控制器默认
    params.window = 0;
    params.filter_duplicates = 1;
    params.passive = 0;                   // 主动扫描:顺便收扫描响应
    const int rc = ble_gap_disc(s_addr_type, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0 && rc != BLE_HS_EALREADY) ESP_LOGE(TAG, "扫描启动失败 rc=%d", rc);
    return rc;
}

static void discovery_start(void)
{
    if (s_stopping) return;                     // 停机中不重新广播/扫描
    if (s_conn != BLE_HS_CONN_HANDLE_NONE) return;
    (void)advertise();
    (void)scan_start();
    set_state(BSP_BLE_LINK_DISCOVERING);
}

static void discovery_stop(void)
{
    // 地址还没取到说明 host 尚未 sync,这时候调 GAP 停止接口没有意义。
    if (!s_have_own_addr) return;
    (void)ble_gap_adv_stop();
    if (ble_gap_disc_active()) (void)ble_gap_disc_cancel();
}

// ——— GATT 客户端(central 侧)———————————————————————————————————

// CCCD 写成功 = 对端确实会收到本机的通知,这时才算双向就绪。
static int on_cccd_written(uint16_t conn_handle, const struct ble_gatt_error *error,
                           struct ble_gatt_attr *attr, void *arg)
{
    (void)attr;
    (void)arg;
    if (conn_handle != s_conn) return 0;

    if (error->status != 0) {
        ESP_LOGE(TAG, "订阅通知失败 status=%d", error->status);
        (void)ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        return 0;
    }
    set_state(BSP_BLE_LINK_READY);
    return 0;
}

static int on_dsc_disc(uint16_t conn_handle, const struct ble_gatt_error *error,
                       uint16_t chr_val_handle, const struct ble_gatt_dsc *dsc, void *arg)
{
    (void)arg;
    if (conn_handle != s_conn) return 0;

    if (error->status == BLE_HS_EDONE) {
        // EDONE 只是“描述符枚举完了”,并不代表没找到:写订阅之后也会收到它。
        // 只有全程没见过 0x2902 才算链路不可用。
        if (!s_cccd_found) {
            ESP_LOGW(TAG, "对端 TX 特征没有可订阅的描述符");
            (void)ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        }
        return 0;
    }
    if (error->status != 0 || !dsc) return 0;
    if (chr_val_handle != s_peer_tx_val) return 0;

    // CCCD = 0x2902,写 0x0001 打开 notify。
    const ble_uuid16_t cccd = BLE_UUID16_INIT(0x2902);
    if (ble_uuid_cmp(&dsc->uuid.u, &cccd.u) != 0) return 0;
    s_cccd_found = true;

    const uint8_t sub[2] = { 1, 0 };
    const int rc = ble_gattc_write_flat(conn_handle, dsc->handle, sub, sizeof(sub),
                                        on_cccd_written, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "订阅通知失败 rc=%d", rc);
        (void)ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
    }
    return 0;
}

static int on_chr_disc(uint16_t conn_handle, const struct ble_gatt_error *error,
                       const struct ble_gatt_chr *chr, void *arg)
{
    (void)arg;
    if (conn_handle != s_conn) return 0;

    if (error->status == BLE_HS_EDONE) {
        if (!s_peer_rx_found || !s_peer_tx_val) {
            ESP_LOGW(TAG, "对端缺少需要的特征(rx=%d tx=%d)",
                     s_peer_rx_found, s_peer_tx_val);
            (void)ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
            return 0;
        }
        // 只发现 TX 这一条特征的描述符:范围是「TX 值句柄 → TX 之后第一个特征之前」。
        const uint16_t end = s_peer_tx_next_def ? (uint16_t)(s_peer_tx_next_def - 1)
                                               : s_peer_svc_end;
        const int rc = ble_gattc_disc_all_dscs(conn_handle, s_peer_tx_val, end,
                                              on_dsc_disc, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "描述符发现启动失败 rc=%d", rc);
            (void)ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        }
        return 0;
    }
    if (error->status != 0 || !chr) return 0;

    if (ble_uuid_cmp(&chr->uuid.u, &s_rx_uuid.u) == 0) {
        s_peer_rx_val = chr->val_handle;
        s_peer_rx_found = true;
    } else if (ble_uuid_cmp(&chr->uuid.u, &s_tx_uuid.u) == 0) {
        s_peer_tx_val = chr->val_handle;
        // 记下这一条之后紧跟的特征,用来划出描述符的搜索范围。
        s_peer_tx_next_def = 0;
    } else if (s_peer_tx_val && chr->def_handle > s_peer_tx_val && !s_peer_tx_next_def) {
        s_peer_tx_next_def = chr->def_handle;
    }
    return 0;
}

static int on_svc_disc(uint16_t conn_handle, const struct ble_gatt_error *error,
                       const struct ble_gatt_svc *svc, void *arg)
{
    (void)arg;
    if (conn_handle != s_conn) return 0;

    if (error->status == BLE_HS_EDONE) {
        if (!s_peer_svc_start) {
            ESP_LOGW(TAG, "对端没有目标服务");
            (void)ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
            return 0;
        }
        const int rc = ble_gattc_disc_all_chrs(conn_handle, s_peer_svc_start,
                                               s_peer_svc_end, on_chr_disc, NULL);
        if (rc != 0) {
            ESP_LOGE(TAG, "特征发现启动失败 rc=%d", rc);
            (void)ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        }
        return 0;
    }
    if (error->status != 0 || !svc) return 0;

    s_peer_svc_start = svc->start_handle;
    s_peer_svc_end = svc->end_handle;
    return 0;
}

static void central_discover(void)
{
    s_peer_rx_val = 0;
    s_peer_tx_val = 0;
    s_peer_rx_found = false;
    s_peer_svc_start = 0;
    s_peer_svc_end = 0;
    s_peer_tx_next_def = 0;
    s_cccd_found = false;

    const int rc = ble_gattc_disc_svc_by_uuid(s_conn, &s_svc_uuid.u, on_svc_disc, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "服务发现启动失败 rc=%d", rc);
        (void)ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
    }
}

// ——— GATT 服务端(peripheral 侧)———————————————————————————————

// 本机通知通道的可读值:给手机/通用 BLE 工具当“服务识别码”用,不参与本机互通。
#define BSP_BLE_INFO_VERSION 1u

static int on_rx_write(uint16_t conn_handle, uint16_t attr_handle,
                       struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;

    if (ctxt->op != BLE_GATT_ACCESS_OP_WRITE_CHR) return BLE_ATT_ERR_UNLIKELY;

    uint8_t buf[BSP_BLE_LINK_MAX_PAYLOAD];
    const unsigned len = OS_MBUF_PKTLEN(ctxt->om);
    if (len == 0 || len > sizeof(buf)) {
        ESP_LOGW(TAG, "收到非法长度的写入:%u", len);
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    uint16_t out_len = 0;
    if (ble_hs_mbuf_to_flat(ctxt->om, buf, sizeof(buf), &out_len) != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }
    if (s_on_rx) s_on_rx(buf, out_len, s_user);
    return 0;
}

static struct ble_gatt_chr_def s_chrs[3];
static struct ble_gatt_svc_def s_svcs[2];

// 通知通道的读取回调:NimBLE 要求每个 characteristic 都有 access_cb
// (ble_gatts_chr_is_sane 会直接判非法),而 notify 本身并不需要回调。
// 这里返回一个小信息对,顺便让 BLE 工具能识别这是谁的通道。
static int on_tx_read(uint16_t conn_handle, uint16_t attr_handle,
                      struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    (void)conn_handle;
    (void)attr_handle;
    (void)arg;
    if (ctxt->op != BLE_GATT_ACCESS_OP_READ_CHR) return BLE_ATT_ERR_UNLIKELY;

    // 这两个字节只是给调试用的:不会影响两机之间的报文。
    const uint8_t info[2] = { BSP_BLE_INFO_VERSION, (uint8_t)(s_is_central ? 1 : 0) };
    return os_mbuf_append(ctxt->om, info, sizeof(info)) == 0 ? 0
                                                            : BLE_ATT_ERR_INSUFFICIENT_RES;
}

// 建 GATT 服务定义。返回 ESP_FAIL 表示定义不合法 —— 这种情况 NimBLE 只会回
// 一个笼统的 EINVAL 错误码,所以在这里就把“哪一条特征不对”说清楚。
static esp_err_t build_gatt_service(void)
{
    memset(s_chrs, 0, sizeof(s_chrs));
    memset(s_svcs, 0, sizeof(s_svcs));

    s_chrs[0].uuid = &s_rx_uuid.u;
    s_chrs[0].access_cb = on_rx_write;
    s_chrs[0].flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP;

    s_chrs[1].uuid = &s_tx_uuid.u;
    s_chrs[1].access_cb = on_tx_read;
    s_chrs[1].flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY;
    s_chrs[1].val_handle = &s_own_tx_val;
    // 终止项 s_chrs[2] 保持全 0。

    s_svcs[0].type = BLE_GATT_SVC_TYPE_PRIMARY;
    s_svcs[0].uuid = &s_svc_uuid.u;
    s_svcs[0].characteristics = s_chrs;
    // 终止项 s_svcs[1] 保持全 0。

    for (int i = 0; i < 2; i++) {
        if (!s_chrs[i].uuid) break;
        if (!s_chrs[i].access_cb) {
            ESP_LOGE(TAG, "特征 %d 缺少 access_cb:NimBLE 会以 EINVAL 拒绝整个服务", i);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

// ——— 事件 ———————————————————————————————————————————————

static void on_connect(const struct ble_gap_event *event)
{
    if (s_stopping) return;
    if (event->connect.status != 0) {
        ESP_LOGW(TAG, "连接失败 status=%d", event->connect.status);
        s_connecting = false;
        discovery_start();
        return;
    }

    s_conn = event->connect.conn_handle;
    s_is_central = s_connecting;
    s_connecting = false;
    s_peer_subscribed = false;
    (void)ble_gap_adv_stop();       // 只留一条连接
    if (ble_gap_disc_active()) (void)ble_gap_disc_cancel();

    ESP_LOGI(TAG, "已连接 handle=%d 角色=%s 空闲堆=%u", s_conn,
             s_is_central ? "central" : "peripheral",
             (unsigned)esp_get_free_heap_size());
    set_state(BSP_BLE_LINK_CONNECTING);

    if (s_is_central) {
        central_discover();
    } else {
        // 本机不是主动方:等对端发现并订阅通知。对端可能已经订阅过了,
        // 但只要没收到 SUBSCRIBE 事件就说明它还没走到那一步。
        s_peer_subscribed = false;
    }
}

static void on_disconnect(const struct ble_gap_event *event)
{
    ESP_LOGI(TAG, "对端断开 reason=%d 空闲堆=%u", event->disconnect.reason,
             (unsigned)esp_get_free_heap_size());
    link_reset();
    if (s_stopping) return;         // 停机触发的断开:清状态即可,不要重新开始发现
    discovery_start();              // 断开后回到“广播 + 扫描”,便于重连
}

static void on_subscribe(const struct ble_gap_event *event)
{
    if (s_stopping) return;
    if (event->subscribe.attr_handle != s_own_tx_val) return;
    s_peer_subscribed = event->subscribe.cur_notify != 0;
    ESP_LOGI(TAG, "对端订阅通知=%d", (int)s_peer_subscribed);
    if (s_peer_subscribed) set_state(BSP_BLE_LINK_READY);
}

static void on_notify_rx(const struct ble_gap_event *event)
{
    if (s_stopping) return;         // 停止期间不再回调应用(与头文件约定一致)
    if (!s_is_central || event->notify_rx.conn_handle != s_conn) return;
    if (event->notify_rx.attr_handle != s_peer_tx_val) return;
    if (!s_on_rx) return;

    uint8_t buf[BSP_BLE_LINK_MAX_PAYLOAD];
    const unsigned len = OS_MBUF_PKTLEN(event->notify_rx.om);
    if (len == 0 || len > sizeof(buf)) {
        ESP_LOGW(TAG, "通知长度非法:%u", len);
        return;
    }
    uint16_t out_len = 0;
    if (ble_hs_mbuf_to_flat(event->notify_rx.om, buf, sizeof(buf), &out_len) != 0) return;
    s_on_rx(buf, out_len, s_user);
}

static void on_adv_or_disc_complete(const struct ble_gap_event *event)
{
    // 广播/扫描结束后重新开始(例如广播被控制器提前结束)。
    if (s_stopping) return;
    if (s_conn != BLE_HS_CONN_HANDLE_NONE || s_connecting) return;
    if (event->type == BLE_GAP_EVENT_ADV_COMPLETE) (void)advertise();
    else (void)scan_start();
}

static void on_disc(const struct ble_gap_event *event)
{
    // 已经连上或正在主动连接时,扫描的残留报告直接忽略。
    if (s_stopping) return;
    if (s_conn != BLE_HS_CONN_HANDLE_NONE || s_connecting) return;

    struct ble_hs_adv_fields fields;
    if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) != 0) {
        return;
    }

    // 只认名字前缀匹配的对端。
    const size_t prefix_len = strlen(s_name_prefix);
    if (!fields.name || fields.name_len <= prefix_len) return;
    if (memcmp(fields.name, s_name_prefix, prefix_len) != 0) return;

    // 反对称的发起规则:地址大的一方主动连。两边算出的结果必然相反。
    // 注意别用 memcmp():它从地址的最低字节开始比,和“扫描器里看到的地址大小”
    // 不是同一个顺序。
    if (!s_have_own_addr) return;
    if (!bsp_ble_addr_should_initiate(s_own_addr.val, event->disc.addr.val)) return;

    // 广播里的名字不一定以 NUL 结尾,先拷成字符串再打日志。
    char peer_name[BSP_BLE_NAME_MAX];
    const size_t copy_len =
        fields.name_len < sizeof(peer_name) - 1 ? fields.name_len : sizeof(peer_name) - 1;
    memcpy(peer_name, fields.name, copy_len);
    peer_name[copy_len] = '\0';
    ESP_LOGI(TAG, "发现对端 %s rssi=%d,由本机发起连接", peer_name, (int)event->disc.rssi);

    if (ble_gap_disc_active()) (void)ble_gap_disc_cancel();
    s_connecting = true;
    const int rc = ble_gap_connect(s_addr_type, &event->disc.addr,
                                   BSP_BLE_CONNECT_TIMEOUT_MS, &s_conn_params,
                                   gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "发起连接失败 rc=%d", rc);
        s_connecting = false;
        (void)scan_start();
        return;
    }
    set_state(BSP_BLE_LINK_CONNECTING);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_CONNECT:
        on_connect(event);
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        on_disconnect(event);
        break;
    case BLE_GAP_EVENT_SUBSCRIBE:
        on_subscribe(event);
        break;
    case BLE_GAP_EVENT_NOTIFY_RX:
        on_notify_rx(event);
        break;
    case BLE_GAP_EVENT_DISC:
        on_disc(event);
        break;
    case BLE_GAP_EVENT_DISC_COMPLETE:
    case BLE_GAP_EVENT_ADV_COMPLETE:
        on_adv_or_disc_complete(event);
        break;
    default:
        break;
    }
    return 0;
}

// ——— host 生命周期 ————————————————————————————————————————————

static void on_reset(int reason)
{
    ESP_LOGE(TAG, "NimBLE reset reason=%d", reason);
}

static void on_sync(void)
{
    // 停机过程中完成的 sync 不要再开始发现(否则同样会踩 host 已关闭的路径)。
    if (s_stopping) return;
    if (ble_hs_util_ensure_addr(0) != 0) {
        ESP_LOGE(TAG, "取本地 BLE 地址失败");
        set_state(BSP_BLE_LINK_FAILED);
        return;
    }
    if (ble_hs_id_infer_auto(0, &s_addr_type) != 0) {
        ESP_LOGE(TAG, "推断地址类型失败");
        set_state(BSP_BLE_LINK_FAILED);
        return;
    }
    if (ble_hs_id_copy_addr(s_addr_type, s_own_addr.val, NULL) != 0) {
        ESP_LOGE(TAG, "读取本机地址失败");
        set_state(BSP_BLE_LINK_FAILED);
        return;
    }
    s_own_addr.type = s_addr_type;
    s_have_own_addr = true;

    discovery_start();
}

// BLE 控制器/PHY 会从 NVS 读射频校准数据;没初始化 NVS 时每次启动都要做全量校准
// (启动更慢、开机电流更高),日志里会看到 esp_phy_load_cal_data_from_nvs 报错。
// 这里只负责初始化,不自动擦除 —— 应用可能已经往 NVS 里存了东西,失败降级即可。
static void ensure_nvs_for_radio(void)
{
    const esp_err_t err = nvs_flash_init();
    if (err == ESP_OK) return;
    ESP_LOGW(TAG, "NVS 初始化失败(%s),射频校准数据将退回全量校准", esp_err_to_name(err));
}

static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    // 不用 port 自带的封装:它把任务句柄私有化,创建失败也不上报。
    // 这里由本模块持有创建、停止确认和删除。
    xSemaphoreGive(s_host_stopped);
    for (;;) vTaskSuspend(NULL);
}

esp_err_t bsp_ble_link_start(const bsp_ble_link_cfg_t *cfg,
                             bsp_ble_link_state_cb_t on_state,
                             bsp_ble_link_rx_cb_t on_rx,
                             void *user)
{
    if (!cfg || !cfg->local_name || !cfg->name_prefix) return ESP_ERR_INVALID_ARG;
    if (s_started) return ESP_ERR_INVALID_STATE;

    // 名字与 UUID 全部拷进本模块的静态存储,调用方之后可以释放自己的副本。
    if (strlen(cfg->local_name) >= sizeof(s_local_name) ||
        strlen(cfg->name_prefix) >= sizeof(s_name_prefix)) {
        return ESP_ERR_INVALID_ARG;
    }
    strcpy(s_local_name, cfg->local_name);
    strcpy(s_name_prefix, cfg->name_prefix);
    memcpy(s_svc_uuid.value, cfg->service_uuid, 16);
    memcpy(s_rx_uuid.value, cfg->rx_uuid, 16);
    memcpy(s_tx_uuid.value, cfg->tx_uuid, 16);
    s_svc_uuid.u.type = BLE_UUID_TYPE_128;
    s_rx_uuid.u.type = BLE_UUID_TYPE_128;
    s_tx_uuid.u.type = BLE_UUID_TYPE_128;

    s_on_state = on_state;
    s_on_rx = on_rx;
    s_user = user;
    s_host_done = false;
    s_stop_in_progress = false;
    s_stopping = false;
    link_reset();
    if (build_gatt_service() != ESP_OK) {
        set_state(BSP_BLE_LINK_FAILED);
        return ESP_FAIL;
    }

    ensure_nvs_for_radio();

    esp_err_t err = nimble_port_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init 失败: %s", esp_err_to_name(err));
        set_state(BSP_BLE_LINK_FAILED);
        return err;
    }
    s_started = true;

    s_host_stopped = xSemaphoreCreateBinary();
    if (!s_host_stopped) {
        err = ESP_ERR_NO_MEM;
        goto failed_start;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_svc_gap_device_name_set(s_local_name);
    if (rc == 0) rc = ble_gatts_count_cfg(s_svcs);
    if (rc == 0) rc = ble_gatts_add_svcs(s_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "GATT 服务注册失败 rc=%d", rc);
        err = ESP_FAIL;
        goto failed_start;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;

    ESP_LOGI(TAG, "BLE 启动前空闲堆=%u,设备名 %s", (unsigned)esp_get_free_heap_size(),
             s_local_name);
    if (xTaskCreatePinnedToCore(host_task, "ble_link_host", NIMBLE_HS_STACK_SIZE,
                               NULL, configMAX_PRIORITIES - 4, &s_host_task,
                               NIMBLE_CORE) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto failed_start;
    }
    set_state(BSP_BLE_LINK_DISCOVERING);
    return ESP_OK;

failed_start:
    // 还没有 host 任务时 nimble_port_stop() 会返回 BLE_HS_EALREADY,直接 deinit。
    {
        const esp_err_t cleanup = bsp_ble_link_stop();
        if (cleanup != ESP_OK) {
            ESP_LOGE(TAG, "NimBLE 启动回滚失败: %s", esp_err_to_name(cleanup));
        }
    }
    set_state(BSP_BLE_LINK_FAILED);
    return err;
}

esp_err_t bsp_ble_link_stop(void)
{
    if (!s_started) return ESP_OK;

    // 停止期间不再向上层报状态:上层可能已经在关机流程里。s_stopping 必须在停
    // 广播和停 host 之前置位 —— ble_hs_stop() 会终止当前连接,随之而来的
    // DISCONNECT 是在 host 已经关闭之后才回调到这里,那时再 discovery_start()
    // 只会拿到 BLE_HS_EDISABLED。
    s_stopping = true;
    s_on_state = NULL;
    discovery_stop();

    if (s_host_task && !s_stop_in_progress) {
        const int rc = nimble_port_stop();
        if (rc != 0) {
            ESP_LOGE(TAG, "nimble_port_stop 失败: %d;链路仍在停机中,需重试 stop()", rc);
            return ESP_FAIL;
        }
        s_stop_in_progress = true;
    }

    if (s_host_task && !s_host_done) {
        if (!s_host_stopped ||
            xSemaphoreTake(s_host_stopped, pdMS_TO_TICKS(BSP_BLE_STOP_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "等待 NimBLE host 停止超时;链路仍在停机中,需重试 stop()");
            return ESP_ERR_TIMEOUT;
        }
        s_host_done = true;
    }

    if (s_host_task) {
        vTaskDelete(s_host_task);
        s_host_task = NULL;
    }

    const esp_err_t err = nimble_port_deinit();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_deinit 失败: %s", esp_err_to_name(err));
        return err;
    }

    if (s_host_stopped) {
        vSemaphoreDelete(s_host_stopped);
        s_host_stopped = NULL;
    }
    s_started = false;
    s_stopping = false;
    s_stop_in_progress = false;
    s_host_done = false;
    s_have_own_addr = false;
    s_on_rx = NULL;
    s_user = NULL;
    link_reset();
    s_state = BSP_BLE_LINK_IDLE;
    ESP_LOGI(TAG, "BLE 已停止,空闲堆=%u", (unsigned)esp_get_free_heap_size());
    return ESP_OK;
}

esp_err_t bsp_ble_link_send(const uint8_t *data, uint16_t len)
{
    if (!data || len == 0 || len > BSP_BLE_LINK_MAX_PAYLOAD) return ESP_ERR_INVALID_ARG;
    if (s_state != BSP_BLE_LINK_READY || s_conn == BLE_HS_CONN_HANDLE_NONE) {
        return ESP_ERR_INVALID_STATE;
    }

    if (s_is_central) {
        // 对端是服务端:写它的接收特征(无响应),APP 层用序号/ACK 保证可靠。
        const int rc = ble_gattc_write_no_rsp_flat(s_conn, s_peer_rx_val, data, len);
        return rc == 0 ? ESP_OK : ESP_FAIL;
    }

    if (!s_peer_subscribed) return ESP_ERR_INVALID_STATE;
    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return ESP_ERR_NO_MEM;
    const int rc = ble_gatts_notify_custom(s_conn, s_own_tx_val, om);
    return rc == 0 ? ESP_OK : ESP_FAIL;
}

bool bsp_ble_link_ready(void)
{
    return s_state == BSP_BLE_LINK_READY;
}

bool bsp_ble_link_is_central(void)
{
    return s_is_central;
}
