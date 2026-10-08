// main/finder_scan.c —— NimBLE 观察者（被动扫描）。见 finder_scan.h。
//
// 协议栈生命周期照抄 main/demo_ble.c 的骨架（那里已经在真机上验证过）：
// nvs 准备 → nimble_port_init → 自建 host 任务 → 停止握手 → 删除任务 → deinit。
// 唯一的结构差别是广播换成扫描（ble_gap_adv_* → ble_gap_disc）。
#include "finder_scan.h"

#include "demo_radio.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <string.h>

static const char *TAG = "finder_scan";

#define FINDER_SCAN_STOP_TIMEOUT_MS 2000
#define FINDER_SCAN_DEVICE_NAME     "FoloPassport"

// BLE 时间单位是 0.625ms；window 与 itvl 必须落在 uint16 内。
#define FINDER_SCAN_BLE_UNITS(ms) ((uint16_t)(((ms) * 1000) / 625))

static SemaphoreHandle_t s_host_stopped;
static TaskHandle_t      s_host_task;
static finder_scan_cb_t  s_cb;
static void             *s_user;
static uint8_t           s_addr_type;
static bool              s_initialized;
static volatile bool     s_scan_requested;
static bool              s_stop_in_progress;
static bool              s_host_done;
static volatile int      s_error;

static int gap_event(struct ble_gap_event *event, void *arg);

static int start_discovery(void)
{
    struct ble_gap_disc_params params;
    memset(&params, 0, sizeof(params));
    params.itvl = FINDER_SCAN_BLE_UNITS(FINDER_SCAN_INTERVAL_MS);
    params.window = FINDER_SCAN_BLE_UNITS(FINDER_SCAN_WINDOW_MS);
    params.filter_policy = 0;
    params.limited = 0;
    // 被动扫描：不发 SCAN_REQ，不给周围的人造成可观测的射频打扰。
    params.passive = 1;
    // 关键：必须保留重复上报。控制器若做重复过滤，同一台设备每轮只上报一次，
    // 连续样本就断了，滤波器和趋势判定会一起失效。
    params.filter_duplicates = 0;

    int rc = ble_gap_disc(s_addr_type, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc != 0) {
        s_error = rc;
        ESP_LOGE(TAG, "ble_gap_disc 失败: %d", rc);
    }
    return rc;
}

static void deliver(const struct ble_gap_disc_desc *disc)
{
    if (!s_cb || !disc) {
        return;
    }
    finder_scan_result_t out;
    memset(&out, 0, sizeof(out));
    memcpy(out.addr, disc->addr.val, 6);
    out.rssi = disc->rssi;

    struct ble_hs_adv_fields fields;
    memset(&fields, 0, sizeof(fields));
    if (ble_hs_adv_parse_fields(&fields, disc->data, disc->length_data) == 0 &&
        fields.name != NULL && fields.name_len > 0) {
        size_t n = fields.name_len;
        if (n > FINDER_SCAN_NAME_MAX) {
            n = FINDER_SCAN_NAME_MAX;
        }
        memcpy(out.name, fields.name, n);
        out.name[n] = '\0';
        out.has_name = true;
    }

    // 回调运行在 NimBLE host 任务里，调用方只能入队。
    s_cb(&out, s_user);
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    switch (event->type) {
    case BLE_GAP_EVENT_DISC:
        deliver(&event->disc);
        return 0;
    case BLE_GAP_EVENT_DISC_COMPLETE:
        // duration 是 BLE_HS_FOREVER，理论上不会到这里。真到了就重启，
        // 以维持"连续扫描"的语义，而不是让界面永远停在旧数据上。
        if (s_scan_requested) {
            (void)start_discovery();
        }
        return 0;
    default:
        return 0;
    }
}

static void on_reset(int reason)
{
    s_error = reason;
    ESP_LOGE(TAG, "NimBLE reset: %d", reason);
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) {
        rc = ble_hs_id_infer_auto(0, &s_addr_type);
    }
    if (rc == 0 && s_scan_requested) {
        rc = start_discovery();
    }
    if (rc != 0) {
        s_error = rc;
        ESP_LOGE(TAG, "NimBLE sync 后启动扫描失败: %d", rc);
    }
}

static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    // 不用 port wrapper：它自持任务句柄且忽略任务创建失败。
    // 本模块自己负责创建、确认与删除。
    xSemaphoreGive(s_host_stopped);
    for (;;) {
        vTaskSuspend(NULL);
    }
}

esp_err_t finder_scan_start(finder_scan_cb_t cb, void *user)
{
    if (s_initialized) {
        s_error = ESP_ERR_INVALID_STATE;
        return ESP_ERR_INVALID_STATE;
    }

    s_cb = cb;
    s_user = user;
    s_stop_in_progress = false;
    s_host_done = false;
    s_error = 0;

    esp_err_t err = demo_radio_nvs_prepare();
    if (err != ESP_OK) {
        s_error = err;
        return err;
    }

    err = nimble_port_init();
    if (err != ESP_OK) {
        s_error = err;
        return err;
    }
    s_initialized = true;

    s_host_stopped = xSemaphoreCreateBinary();
    if (!s_host_stopped) {
        err = ESP_ERR_NO_MEM;
        goto failed_start;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();
    if (ble_svc_gap_device_name_set(FINDER_SCAN_DEVICE_NAME) != 0) {
        err = ESP_FAIL;
        goto failed_start;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    s_scan_requested = true;

    if (xTaskCreatePinnedToCore(host_task, "finder_ble", NIMBLE_HS_STACK_SIZE, NULL,
                                configMAX_PRIORITIES - 4, &s_host_task,
                                NIMBLE_CORE) != pdPASS) {
        err = ESP_ERR_NO_MEM;
        goto failed_start;
    }
    return ESP_OK;

failed_start:
    // host 任务不存在，此时 nimble_port_stop() 会返回 BLE_HS_EALREADY，
    // 直接反初始化；若清理本身失败则保留所有权以便重试。
    s_scan_requested = false;
    esp_err_t cleanup = finder_scan_stop();
    if (cleanup != ESP_OK) {
        ESP_LOGE(TAG, "启动失败后的清理也未完成: %s", esp_err_to_name(cleanup));
    }
    s_error = err;
    return err;
}

esp_err_t finder_scan_stop(void)
{
    s_scan_requested = false;
    if (!s_initialized) {
        return ESP_OK;
    }

    if (s_host_task && !s_stop_in_progress) {
        (void)ble_gap_disc_cancel();
        int rc = nimble_port_stop();
        if (rc != 0) {
            ESP_LOGE(TAG, "nimble_port_stop 失败: %d", rc);
            s_error = rc;
            return ESP_FAIL;
        }
        s_stop_in_progress = true;
    }

    if (s_host_task && !s_host_done) {
        if (!s_host_stopped ||
            xSemaphoreTake(s_host_stopped, pdMS_TO_TICKS(FINDER_SCAN_STOP_TIMEOUT_MS)) != pdTRUE) {
            ESP_LOGE(TAG, "等待 NimBLE host 停止超时");
            s_error = ESP_ERR_TIMEOUT;
            return ESP_ERR_TIMEOUT;
        }
        s_host_done = true;
    }

    if (s_host_task) {
        vTaskDelete(s_host_task);
        s_host_task = NULL;
    }

    esp_err_t err = nimble_port_deinit();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_deinit 失败: %s", esp_err_to_name(err));
        s_error = err;
        return err;
    }

    s_initialized = false;
    s_stop_in_progress = false;
    s_host_done = false;
    if (s_host_stopped) {
        vSemaphoreDelete(s_host_stopped);
        s_host_stopped = NULL;
    }
    return ESP_OK;
}

bool finder_scan_running(void)
{
    return s_initialized;
}

int finder_scan_last_error(void)
{
    return s_error;
}
