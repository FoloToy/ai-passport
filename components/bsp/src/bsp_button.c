// components/bsp/src/bsp_button.c
// 移植自 trae_card/components/platform/platform_esp32/src/btn_iot_button.c
#include "bsp_button.h"
#include "bsp_pins.h"
#include "iot_button.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "bsp_btn";

static const uint16_t BTN_MV[BSP_BTN_COUNT][2] = BSP_BTN_MV_TABLE;

static button_handle_t s_btn[BSP_BTN_COUNT];
static bsp_btn_cb_t    s_cb;
static void           *s_user;
static volatile bool   s_ready;

// ADC1 是 unit 级独占资源:iot_button 与 bsp_button_read_mv() 必须共用同一个 oneshot
// 句柄。谁第二个调 adc_oneshot_new_unit() 谁就拿到 "adc1 is already in use"。
static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t         s_cali;

#define BSP_BTN_ATTEN  ADC_ATTEN_DB_12       // 量程约 0~3100mV,覆盖松开态

// Use the public button-driver interface, not button_adc's global registry:
// button 4.2.0 leaves an occupied index behind when its core allocation fails.
// These static drivers and the single ADC/calibration pair are owned by BSP.
typedef struct {
    button_driver_t base;
    unsigned index;
} bsp_adc_button_t;
static bsp_adc_button_t s_drivers[BSP_BTN_COUNT];
static int64_t s_sample_time;
static int s_sample_mv = -1;
static bool s_sample_valid;

static uint8_t button_level(button_driver_t *driver) {
    if (!s_ready) return BUTTON_INACTIVE;
    const bsp_adc_button_t *button = (const bsp_adc_button_t *)driver;
    const int64_t now = esp_timer_get_time();
    // Share one averaged reading across the three keys in a polling cycle.
    if (!s_sample_valid || now - s_sample_time >= 1000) {
        int sum = 0;
        s_sample_time = now;
        s_sample_valid = true;
        s_sample_mv = -1;
        for (int i = 0; i < CONFIG_ADC_BUTTON_SAMPLE_TIMES; ++i) {
            int raw;
            if (adc_oneshot_read(s_adc, BSP_BTN_ADC_CHANNEL, &raw) != ESP_OK) {
                return BUTTON_INACTIVE;
            }
            sum += raw;
        }
        if (adc_cali_raw_to_voltage(s_cali, sum / CONFIG_ADC_BUTTON_SAMPLE_TIMES,
                                   &s_sample_mv) != ESP_OK) {
            s_sample_mv = -1;
        }
    }
    // Half-open windows prevent two keys from matching a shared boundary.
    return s_sample_mv >= BTN_MV[button->index][0] &&
           s_sample_mv < BTN_MV[button->index][1] ? BUTTON_ACTIVE : BUTTON_INACTIVE;
}

static esp_err_t button_driver_delete(button_driver_t *driver) {
    (void)driver; // Static storage; shared ADC is released after all buttons.
    return ESP_OK;
}

// 每个按键把"哪个键"随回调带回来。button 组件的回调签名固定,故用 usr_data 传索引。
static void on_event(void *arg, void *usr_data, bsp_btn_ev_t ev) {
    (void)arg;
    if (!s_ready || !s_cb) return;
    s_cb((bsp_btn_t)(intptr_t)usr_data, ev, s_user);
}
static void cb_press (void *a, void *u) { on_event(a, u, BSP_BTN_PRESS);  }
static void cb_click (void *a, void *u) { on_event(a, u, BSP_BTN_CLICK);  }
static void cb_double(void *a, void *u) { on_event(a, u, BSP_BTN_DOUBLE); }
static void cb_long  (void *a, void *u) { on_event(a, u, BSP_BTN_LONG);   }

// 初始化中途失败时先停掉所有 button driver，再释放本文件持有的校准与 ADC unit。
// button driver 仍在轮询时不能先删 ADC，否则 timer callback 会访问失效句柄。
static void button_cleanup(void) {
    s_cb = NULL;
    s_user = NULL;
    s_ready = false;
    s_sample_valid = false;

    for (int i = BSP_BTN_COUNT - 1; i >= 0; i--) {
        if (!s_btn[i]) continue;
        esp_err_t e = iot_button_delete(s_btn[i]);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "按键 %d 回滚失败: %s", i, esp_err_to_name(e));
            continue;
        }
        s_btn[i] = NULL;
    }

    // Never free the ADC beneath a driver whose deletion failed.
    for (int i = 0; i < BSP_BTN_COUNT; ++i) {
        if (s_btn[i]) return;
    }

    if (s_cali) {
        esp_err_t e = adc_cali_delete_scheme_curve_fitting(s_cali);
        if (e != ESP_OK) ESP_LOGE(TAG, "ADC 校准回滚失败: %s", esp_err_to_name(e));
        else s_cali = NULL;
    }
    if (s_adc) {
        esp_err_t e = adc_oneshot_del_unit(s_adc);
        if (e != ESP_OK) ESP_LOGE(TAG, "ADC unit 回滚失败: %s", esp_err_to_name(e));
        else s_adc = NULL;
    }
}

static esp_err_t register_callbacks(button_handle_t button, void *index) {
    esp_err_t e = iot_button_register_cb(button, BUTTON_PRESS_DOWN, NULL, cb_press, index);
    if (e == ESP_OK) e = iot_button_register_cb(button, BUTTON_SINGLE_CLICK, NULL, cb_click, index);
    if (e == ESP_OK) e = iot_button_register_cb(button, BUTTON_DOUBLE_CLICK, NULL, cb_double, index);
    if (e == ESP_OK) e = iot_button_register_cb(button, BUTTON_LONG_PRESS_START, NULL, cb_long, index);
    return e;
}

esp_err_t bsp_button_init(bsp_btn_cb_t cb, void *user) {
    if (s_ready) {
        s_cb = cb;
        s_user = user;
        return ESP_OK;
    }
    if (s_adc || s_cali) {
        ESP_LOGE(TAG, "上次按键初始化回滚不完整，拒绝覆盖仍存活的 ADC 句柄");
        return ESP_ERR_INVALID_STATE;
    }
    for (int i = 0; i < BSP_BTN_COUNT; i++) {
        if (s_btn[i]) {
            ESP_LOGE(TAG, "上次按键 %d 回滚不完整，拒绝重复分配资源", i);
            return ESP_ERR_INVALID_STATE;
        }
    }

    s_cb = cb; s_user = user;

    // BSP owns one ADC unit and one calibration handle for polling and reads.
    const adc_oneshot_unit_init_cfg_t ucfg = { .unit_id = BSP_BTN_ADC_UNIT };
    esp_err_t ae = adc_oneshot_new_unit(&ucfg, &s_adc);
    if (ae != ESP_OK) {
        ESP_LOGE(TAG, "ADC unit 创建失败 (%s)", esp_err_to_name(ae));
        s_adc = NULL;
        button_cleanup();
        return ae;
    }

    const adc_oneshot_chan_cfg_t channel = {
        .atten = BSP_BTN_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ae = adc_oneshot_config_channel(s_adc, BSP_BTN_ADC_CHANNEL, &channel);
    if (ae != ESP_OK) { button_cleanup(); return ae; }
    const adc_cali_curve_fitting_config_t cal = {
        .unit_id = BSP_BTN_ADC_UNIT,
        .chan = BSP_BTN_ADC_CHANNEL,
        .atten = BSP_BTN_ATTEN,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ae = adc_cali_create_scheme_curve_fitting(&cal, &s_cali);
    if (ae != ESP_OK) {
        ESP_LOGE(TAG, "ADC 校准创建失败: %s", esp_err_to_name(ae));
        button_cleanup();
        return ae; // No guessed mV: an unavailable reading is not an UP press.
    }

    for (int i = 0; i < BSP_BTN_COUNT; i++) {
        s_drivers[i] = (bsp_adc_button_t){
            .base = { .get_key_level = button_level, .del = button_driver_delete },
            .index = (unsigned)i,
        };
        // 判定门限由 BSP 显式下发(见 bsp_pins.h):组件 Kconfig 默认的长按 1500ms 偏迟钝。
        const button_config_t bc = {
            .short_press_time = BSP_BTN_SHORT_PRESS_MS,
            .long_press_time  = BSP_BTN_LONG_PRESS_MS,
        };
        esp_err_t e = iot_button_create(&bc, &s_drivers[i].base, &s_btn[i]);
        if (e != ESP_OK || !s_btn[i]) {
            ESP_LOGE(TAG, "按键 %d 创建失败 (%s) —— 检查 GPIO%d 的 ADC 配置与分压电阻",
                     i, esp_err_to_name(e), BSP_BTN_ADC_CHANNEL);
            e = e == ESP_OK ? ESP_FAIL : e;
            button_cleanup();
            return e;
        }
        void *idx = (void *)(intptr_t)i;
        e = register_callbacks(s_btn[i], idx);
        if (e != ESP_OK) {
            ESP_LOGE(TAG, "按键 %d 回调注册失败: %s", i, esp_err_to_name(e));
            button_cleanup();
            return e;
        }
    }

    s_sample_valid = false;
    s_ready = true;
    ESP_LOGI(TAG, "按键就绪:ADC1_CH%d 三键分压,短按 %dms 长按 %dms",
             BSP_BTN_ADC_CHANNEL, BSP_BTN_SHORT_PRESS_MS, BSP_BTN_LONG_PRESS_MS);
    return ESP_OK;
}

int bsp_button_read_mv(void) {
    // 读的是 bsp_button_init() 建好、并与 iot_button 共用的那一路 ADC。
    // 单次采样与组件的按键轮询互不干扰(oneshot 内部自带锁)。
    if (!s_adc || !s_cali) return -1;

    int raw = 0, mv = 0;
    if (adc_oneshot_read(s_adc, BSP_BTN_ADC_CHANNEL, &raw) != ESP_OK) return -1;
    if (adc_cali_raw_to_voltage(s_cali, raw, &mv) != ESP_OK) return -1;
    return mv;
}
