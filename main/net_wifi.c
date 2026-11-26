// main/net_wifi.c —— WiFi STA 联网实现。
// 凭据只有一个来源:NVS("wifi_cfg"),由手机配网写入。
// 固件里不编任何 private WiFi —— 换网络只改 NVS,发出去的固件也不含密码;
// 没有凭据时不启动 STA,但网络栈已初始化好,配网模块可以直接起 SoftAP。
// 断开后用 esp_timer 单次定时器延迟重连,避免在事件任务里忙等。
#include "net_wifi.h"

#include <stdio.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "wifi_store.h"


static const char *TAG = "net_wifi";

#define WIFI_CONNECTED_BIT BIT0
#define RETRY_DELAY_US (3LL * 1000 * 1000)
#define SSID_MAX_LEN 32
#define PASS_MAX_LEN 64

static EventGroupHandle_t s_events;
static esp_timer_handle_t s_retry_timer;
static bool s_stack_ready;
static bool s_started;
static bool s_has_creds;
static char s_ssid[SSID_MAX_LEN + 1];
static char s_pass[PASS_MAX_LEN + 1];

// 只认 NVS:没有记录就是没配过网,交由 UI 引导配网。
// (以前这里还有一条「编译期默认值」兜底,实测不可靠且会把私人 WiFi 编进固件,已去掉。)
static bool load_creds(void) {
    if (wifi_store_load(s_ssid, sizeof(s_ssid), s_pass, sizeof(s_pass))) {
        ESP_LOGI(TAG, "使用已保存的凭据: \"%s\"", s_ssid);
        return true;
    }
    s_ssid[0] = '\0';
    s_pass[0] = '\0';
    return false;
}

static void retry_cb(void *arg) {
    (void)arg;
    ESP_LOGI(TAG, "重连中");
    esp_err_t err = esp_wifi_connect();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "重连请求失败: %s", esp_err_to_name(err));
        if (s_retry_timer) esp_timer_start_once(s_retry_timer, RETRY_DELAY_US);
    }
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)data;
    if (base != WIFI_EVENT) return;
    if (id == WIFI_EVENT_STA_START) {
        // 配网会以 APSTA 启动 WiFi,没凭据时不要盲连(否则一直空转重试)。
        if (s_has_creds) esp_wifi_connect();
    } else if (id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_events) xEventGroupClearBits(s_events, WIFI_CONNECTED_BIT);
        if (!s_has_creds) return;
        ESP_LOGW(TAG, "WiFi 断开,3 秒后重连");
        if (s_retry_timer) esp_timer_start_once(s_retry_timer, RETRY_DELAY_US);
    }
}

static void on_ip_event(void *arg, esp_event_base_t base, int32_t id, void *data) {
    (void)arg;
    (void)data;
    if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        if (s_events) xEventGroupSetBits(s_events, WIFI_CONNECTED_BIT);
        ESP_LOGI(TAG, "已联网");
    }
}

// WiFi 驱动内部要用 NVS 存 PHY 校准数据,必须先初始化它;
// 分区写满或版本不兼容时重建一次分区表,其余错误只降级不重启。
static bool nvs_prepare(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 需要重建: %s", esp_err_to_name(err));
        if (nvs_flash_erase() != ESP_OK) {
            ESP_LOGE(TAG, "NVS 擦除失败");
            return false;
        }
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS 初始化失败: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

// 网络栈只起一次:STA 联网和 SoftAP 配网都走这里。
// 起不来只让开奖页显示离线,绝不 abort 整机。
static bool stack_init(void) {
    if (s_stack_ready) return true;
    if (!nvs_prepare()) return false;
    if (esp_netif_init() != ESP_OK) {
        ESP_LOGE(TAG, "netif 初始化失败");
        return false;
    }
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "事件循环创建失败: %s", esp_err_to_name(err));
        return false;
    }
    if (esp_netif_create_default_wifi_sta() == NULL) {
        ESP_LOGE(TAG, "创建 STA 网络接口失败");
        return false;
    }
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    if (esp_wifi_init(&init_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 初始化失败");
        return false;
    }
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, on_wifi_event, NULL);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, on_ip_event, NULL);

    s_events = xEventGroupCreate();
    const esp_timer_create_args_t timer_args = {
        .callback = retry_cb,
        .name = "wifi_retry",
    };
    if (esp_timer_create(&timer_args, &s_retry_timer) != ESP_OK) {
        ESP_LOGE(TAG, "重连定时器创建失败");
        s_retry_timer = NULL;
    }
    s_stack_ready = true;
    return true;
}

void net_wifi_start(void) {
    if (s_started) return;
    if (!stack_init()) {
        ESP_LOGE(TAG, "网络栈起不来,开奖结果功能保持离线");
        return;
    }
    s_has_creds = load_creds();
    if (!s_has_creds) {
        ESP_LOGW(TAG, "没有 WiFi 凭据,等待手机配网(设置页长按上键)");
        return;
    }

    wifi_config_t wifi_cfg = {0};
    // ssid/password 是定长数组,长度已在 wifi_store 侧校验过;这里用 memcpy 明确截断边界,
    // 换成 snprintf 编译器会报 format-truncation(理论上不可能发生,但警告即错误)。
    size_t ssid_len = strlen(s_ssid);
    if (ssid_len > sizeof(wifi_cfg.sta.ssid)) ssid_len = sizeof(wifi_cfg.sta.ssid);
    memcpy(wifi_cfg.sta.ssid, s_ssid, ssid_len);
    size_t pass_len = strlen(s_pass);
    if (pass_len > sizeof(wifi_cfg.sta.password)) pass_len = sizeof(wifi_cfg.sta.password);
    memcpy(wifi_cfg.sta.password, s_pass, pass_len);
    // 开放网络也能连;有密码时仍优先 WPA2。
    wifi_cfg.sta.threshold.authmode = (s_pass[0] != '\0') ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN;

    s_started = true;
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err == ESP_OK) err = esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg);
    if (err == ESP_OK) err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 启动失败: %s", esp_err_to_name(err));
        s_started = false;
        return;
    }
    ESP_LOGI(TAG, "开始连接 \"%s\"", s_ssid);
}

bool net_wifi_stack_ready(void) {
    return s_stack_ready;
}

bool net_wifi_configured(void) {
    return s_has_creds;
}

const char *net_wifi_ssid(void) {
    return s_ssid;
}

bool net_wifi_connected(void) {
    if (!s_events) return false;
    return (xEventGroupGetBits(s_events) & WIFI_CONNECTED_BIT) != 0;
}
