// main/demo_ble.c —— NimBLE 广播示例；手机可扫描到 FoloPassport。
#include "demo.h"
#include "demo_radio.h"
#include "ui_pixel.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "host/ble_gap.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "lvgl.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include <string.h>

static const char *TAG = "demo_ble";
static const char *DEVICE_NAME = "FoloPassport";

typedef enum {
    BLE_DEMO_OFF = 0,
    BLE_DEMO_STARTING,
    BLE_DEMO_ADVERTISING,
    BLE_DEMO_FAILED,
} ble_demo_state_t;

static lv_obj_t *s_scr;
static lv_obj_t *s_status;
static lv_timer_t *s_timer;
static SemaphoreHandle_t s_host_stopped;
static volatile ble_demo_state_t s_state;
static volatile int s_error;
static uint8_t s_addr_type;
static bool s_initialized;
static bool s_start_requested;

static int gap_event(struct ble_gap_event *event, void *arg);

static int advertise(void)
{
    struct ble_hs_adv_fields fields = { 0 };
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;
    fields.name = (const uint8_t *)DEVICE_NAME;
    fields.name_len = strlen(DEVICE_NAME);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) return rc;

    struct ble_gap_adv_params params = { 0 };
    params.conn_mode = BLE_GAP_CONN_MODE_NON;
    params.disc_mode = BLE_GAP_DISC_MODE_GEN;
    rc = ble_gap_adv_start(s_addr_type, NULL, BLE_HS_FOREVER, &params, gap_event, NULL);
    if (rc == 0) s_state = BLE_DEMO_ADVERTISING;
    return rc;
}

static int gap_event(struct ble_gap_event *event, void *arg)
{
    (void)arg;
    if (event->type == BLE_GAP_EVENT_ADV_COMPLETE && s_start_requested) {
        int rc = advertise();
        if (rc != 0) {
            s_error = rc;
            s_state = BLE_DEMO_FAILED;
        }
    }
    return 0;
}

static void on_reset(int reason)
{
    s_error = reason;
    s_state = BLE_DEMO_FAILED;
}

static void on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc == 0) rc = ble_hs_id_infer_auto(0, &s_addr_type);
    if (rc == 0 && s_start_requested) rc = advertise();
    if (rc != 0) {
        s_error = rc;
        s_state = BLE_DEMO_FAILED;
    }
}

static void host_task(void *arg)
{
    (void)arg;
    nimble_port_run();
    if (s_host_stopped) xSemaphoreGive(s_host_stopped);
    nimble_port_freertos_deinit();
}

static esp_err_t ble_start(void)
{
    if (s_initialized) {
        s_error = ESP_ERR_INVALID_STATE;
        s_state = BLE_DEMO_FAILED;
        return ESP_ERR_INVALID_STATE;
    }

    s_state = BLE_DEMO_STARTING;
    esp_err_t err = demo_radio_nvs_prepare();
    if (err != ESP_OK) {
        s_error = err;
        s_state = BLE_DEMO_FAILED;
        return err;
    }

    err = nimble_port_init();
    if (err != ESP_OK) {
        s_error = err;
        s_state = BLE_DEMO_FAILED;
        return err;
    }
    s_initialized = true;
    s_host_stopped = xSemaphoreCreateBinary();
    if (!s_host_stopped) {
        nimble_port_deinit();
        s_initialized = false;
        s_error = ESP_ERR_NO_MEM;
        s_state = BLE_DEMO_FAILED;
        return ESP_ERR_NO_MEM;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();
    int rc = ble_svc_gap_device_name_set(DEVICE_NAME);
    if (rc != 0) {
        vSemaphoreDelete(s_host_stopped);
        s_host_stopped = NULL;
        nimble_port_deinit();
        s_initialized = false;
        s_error = rc;
        s_state = BLE_DEMO_FAILED;
        return ESP_FAIL;
    }

    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;
    s_start_requested = true;
    nimble_port_freertos_init(host_task);
    return ESP_OK;
}

static void ble_stop(void)
{
    s_start_requested = false;
    if (!s_initialized) return;

    ble_gap_adv_stop();
    int rc = nimble_port_stop();
    if (rc == 0 && s_host_stopped) {
        // host callback 不访问 LVGL；即使页面 exit 持有 LVGL 锁也不会形成锁环。
        xSemaphoreTake(s_host_stopped, portMAX_DELAY);
    }
    if (rc == 0) {
        nimble_port_deinit();
        s_initialized = false;
    } else {
        ESP_LOGE(TAG, "nimble_port_stop 失败: %d", rc);
    }
    if (!s_initialized && s_host_stopped) {
        vSemaphoreDelete(s_host_stopped);
        s_host_stopped = NULL;
    }
    s_state = BLE_DEMO_OFF;
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    switch (s_state) {
    case BLE_DEMO_STARTING:
        lv_label_set_text(s_status, "Starting NimBLE...");
        break;
    case BLE_DEMO_ADVERTISING:
        lv_label_set_text(s_status, "ADVERTISING\n\nName: FoloPassport\n\nUse a BLE scanner\non your phone.\n\nOK: RESTART ADV");
        break;
    case BLE_DEMO_FAILED:
        lv_label_set_text_fmt(s_status, "BLE failed: %d", s_error);
        s_state = BLE_DEMO_OFF;
        break;
    default:
        break;
    }
}

void demo_ble_enter(void)
{
    s_scr = ui_pixel_screen_create("BLUETOOTH LE");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 22, 58, 196, 180, UI_PAPER);
    s_status = lv_label_create(panel);
    lv_obj_set_width(s_status, 168);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(UI_INK), 0);
    lv_obj_center(s_status);
    lv_label_set_text(s_status, "Starting NimBLE...");
    ui_pixel_mascot_create(s_scr, 101, 244);
    s_timer = lv_timer_create(tick, 100, NULL);
    lv_screen_load(s_scr);
    ble_start();
}

void demo_ble_exit(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    ble_stop();
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_status = NULL;
    }
}

void demo_ble_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (btn != BSP_BTN_OK || ev != BSP_BTN_CLICK || !s_initialized) return;
    ble_gap_adv_stop();
    int rc = advertise();
    if (rc != 0) {
        s_error = rc;
        s_state = BLE_DEMO_FAILED;
    }
}
