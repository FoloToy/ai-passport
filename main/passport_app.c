// main/passport_app.c —— 创智学员 Passport 应用主体。
//
// 省电（实现文档第 9 节）：15s 无操作关背光；30s 进浅睡
// （RTC 10 分钟定时 + GPIO0 按键唤醒）；定时唤醒只做心跳突发，
// 完成即回睡。心跳周期 10 分钟 < 服务端 15 分钟惰性关闭阈值。
#include "passport_app.h"

#include "passport_core.h"
#include "passport_net.h"
#include "passport_store.h"
#include "passport_ui.h"

#include "bsp_button.h"
#include "bsp_display.h"
#include "driver/gpio.h"
#include "esp_app_desc.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *TAG = "smpass_app";

#define KEY_QUEUE_LEN      8
#define UI_QUEUE_LEN       8
#define BACKLIGHT_OFF_MS   15000
#define LIGHT_SLEEP_MS     30000
#define HEARTBEAT_PERIOD_S 600           // 10 分钟（实现文档 6.1 节）
#define SYNC_FALLBACK_S    3600          // 每小时兜底同步
#define BOOT_SYNC_DELAY_S  3
#define HEARTBEAT_BURST_TIMEOUT_MS 20000

typedef struct {
    bsp_btn_t btn;
    bsp_btn_ev_t ev;
} key_evt_t;

typedef enum {
    MODE_PASSPORT = 0,
    MODE_LEGACY_DEMO,
} app_mode_t;

static QueueHandle_t s_key_queue;
static QueueHandle_t s_ui_queue;
static TaskHandle_t  s_power_task;
static app_mode_t    s_mode = MODE_PASSPORT;
static int64_t       s_last_activity_us;
static int64_t       s_last_heartbeat_s;
static int64_t       s_last_sync_s;
static bool          s_backlight_on = true;
static bool          s_boot_sync_done;
static bool          s_sleeping;

// ---------------------------------------------------------------------------
// 按键回调：只投递队列（上游红线：回调不得阻塞）
// ---------------------------------------------------------------------------
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user)
{
    (void)user;
    if (!s_key_queue) return;
    key_evt_t evt = { .btn = btn, .ev = ev };
    xQueueSend(s_key_queue, &evt, 0);
}

// ---------------------------------------------------------------------------
// 浅睡（独立任务，对齐上游 demo_low_power 模式）
// ---------------------------------------------------------------------------
static void power_task(void *arg)
{
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // 唤醒期间若用户有按键活动，放弃本次入睡（防陈旧 notify 误睡）
        if ((esp_timer_get_time() - s_last_activity_us) / 1000 < LIGHT_SLEEP_MS) {
            continue;
        }

        if (bsp_lvgl_lock(500)) {             // 非 LVGL 上下文，操作 UI 需持锁
            passport_ui_clear_sensitive();    // QR 页睡前清屏（安全红线）
            bsp_lvgl_unlock();
        }
        bsp_display_backlight(0);
        s_backlight_on = false;
        s_sleeping = true;

        // RTC 定时：10 分钟后醒来做心跳突发；GPIO0 低电平（UP 键 0Ω 档）唤醒。
        // 按键唤醒链路需真机验证（上游注明无板级唤醒电路证据，列入 Unverified）。
        esp_sleep_enable_timer_wakeup((uint64_t)HEARTBEAT_PERIOD_S * 1000000ULL);
        gpio_wakeup_enable(GPIO_NUM_0, GPIO_INTR_LOW_LEVEL);   // UP 键 0Ω 档=低电平
        esp_sleep_enable_gpio_wakeup();
        esp_light_sleep_start();
        esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
        s_sleeping = false;

        esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        if (cause == ESP_SLEEP_WAKEUP_TIMER) {
            // 心跳突发：保持熄屏，突发结束立即回睡
            s_last_heartbeat_s = esp_timer_get_time() / 1000000;
            passport_net_request(PASSPORT_NET_REQ_HEARTBEAT);
            int64_t deadline = esp_timer_get_time() + HEARTBEAT_BURST_TIMEOUT_MS * 1000;
            while (passport_net_is_busy() && esp_timer_get_time() < deadline) {
                vTaskDelay(pdMS_TO_TICKS(200));
            }
            // 回睡条件由 dispatch 的 inactivity 检查自然触发（活动时间很旧）
        } else {
            // 按键（或未知原因）唤醒：亮屏回菜单
            bsp_display_backlight(100);
            s_backlight_on = true;
            s_last_activity_us = esp_timer_get_time();
            if (bsp_lvgl_lock(500)) {
                if (s_mode == MODE_PASSPORT) passport_ui_show(PASSPORT_SCREEN_MENU);
                bsp_lvgl_unlock();
            }
        }
    }
}

// ---------------------------------------------------------------------------
// dispatch 定时器（LVGL 任务上下文 = 唯一 UI 上下文）
// ---------------------------------------------------------------------------
static void enter_legacy_demo(void)
{
    s_mode = MODE_LEGACY_DEMO;
    legacy_menu_enter();
}

static void handle_key(key_evt_t evt)
{
    int64_t now = esp_timer_get_time();

    // 熄屏状态下的第一键只负责亮屏，不执行动作
    if (!s_backlight_on) {
        bsp_display_backlight(100);
        s_backlight_on = true;
        s_last_activity_us = now;
        return;
    }
    s_last_activity_us = now;

    if (s_mode == MODE_LEGACY_DEMO) {
        if (legacy_menu_handle_key((int)evt.btn, (int)evt.ev)) {
            s_mode = MODE_PASSPORT;
            passport_ui_show(PASSPORT_SCREEN_MENU);
        }
        return;
    }

    // 全局：OK 长按返回菜单（设置页除外——它的长按留给 5 秒隐藏入口检测）
    if (evt.btn == BSP_BTN_OK && evt.ev == BSP_BTN_LONG &&
        passport_ui_current() != PASSPORT_SCREEN_MENU &&
        passport_ui_current() != PASSPORT_SCREEN_SETTINGS) {
        passport_ui_show(PASSPORT_SCREEN_MENU);
        return;
    }
    if (evt.btn == BSP_BTN_OK && evt.ev == BSP_BTN_LONG &&
        passport_ui_current() == PASSPORT_SCREEN_MENU) {
        return;   // 菜单里 OK 长按无意义
    }

    passport_ui_handle_key(evt.btn, evt.ev);
}

static void second_tick(void)
{
    int64_t now_s = esp_timer_get_time() / 1000000;

    if (s_mode == MODE_PASSPORT) passport_ui_tick();

    // 开机一次性同步
    if (!s_boot_sync_done && now_s >= BOOT_SYNC_DELAY_S) {
        s_boot_sync_done = true;
        s_last_sync_s = now_s;
        if (passport_store_is_provisioned()) {
            passport_net_request(PASSPORT_NET_REQ_SYNC);
        }
    }
    // 每小时兜底同步
    if (s_boot_sync_done && now_s - s_last_sync_s >= SYNC_FALLBACK_S &&
        !passport_net_is_busy()) {
        s_last_sync_s = now_s;
        if (passport_store_is_provisioned()) {
            passport_net_request(PASSPORT_NET_REQ_SYNC);
        }
    }
    // 唤醒状态下也维持 10 分钟心跳节奏
    if (now_s - s_last_heartbeat_s >= HEARTBEAT_PERIOD_S &&
        !passport_net_is_busy()) {
        s_last_heartbeat_s = now_s;
        if (passport_store_is_provisioned()) {
            passport_net_request(PASSPORT_NET_REQ_HEARTBEAT);
        }
    }
}

static void dispatch_cb(lv_timer_t *timer)
{
    (void)timer;
    static int s_tick_div;

    key_evt_t key;
    while (xQueueReceive(s_key_queue, &key, 0) == pdTRUE) {
        handle_key(key);
    }
    passport_ui_evt_t evt;
    while (xQueueReceive(s_ui_queue, &evt, 0) == pdTRUE) {
        if ((int)evt.type == 0 && evt.arg1 == PASSPORT_ACTION_ENTER_DEMO) {
            enter_legacy_demo();   // UI 模块投递的隐藏入口动作
        } else if (s_mode == MODE_PASSPORT) {
            passport_ui_on_net_event(&evt);
        }
    }

    if (++s_tick_div >= 10) {   // 100ms × 10 = 1s
        s_tick_div = 0;
        second_tick();
    }

    // 无操作省电
    if (!s_sleeping) {
        int64_t idle_ms = (esp_timer_get_time() - s_last_activity_us) / 1000;
        if (s_backlight_on && idle_ms >= BACKLIGHT_OFF_MS) {
            bsp_display_backlight(0);
            s_backlight_on = false;
        }
        if (idle_ms >= LIGHT_SLEEP_MS && !passport_net_is_busy() && s_power_task) {
            s_last_activity_us = esp_timer_get_time() - LIGHT_SLEEP_MS * 1000;  // 防抖动重入
            xTaskNotifyGive(s_power_task);
        }
    }
}

// ---------------------------------------------------------------------------
// 产线控制台（USB Serial/JTAG REPL；实现文档 8.1 节）
// 安全红线：任何命令不得回显 secret / sig / 动态码 / Wi-Fi 密码。
// ---------------------------------------------------------------------------
static int hex_to_bytes(const char *hex, uint8_t *out, size_t out_len)
{
    if (strlen(hex) != out_len * 2) return -1;
    for (size_t i = 0; i < out_len; i++) {
        unsigned int v;
        if (sscanf(hex + i * 2, "%2x", &v) != 1) return -1;
        out[i] = (uint8_t)v;
    }
    return 0;
}

static int cmd_smpass(int argc, char **argv)
{
    if (argc < 2) {
        printf("usage:\n"
               "  smpass provision <pid> <64-hex-secret>\n"
               "  smpass wifi set <1|2> <ssid> <password>\n"
               "  smpass status\n"
               "  smpass erase\n");
        return 1;
    }

    if (strcmp(argv[1], "provision") == 0) {
        if (argc != 4) {
            printf("usage: smpass provision <pid> <64-hex-secret>\n");
            return 1;
        }
        if (passport_store_is_provisioned()) {
            printf("ERROR: already provisioned; run `smpass erase` first\n");
            return 1;
        }
        uint8_t secret[PASSPORT_SECRET_LEN];
        if (hex_to_bytes(argv[3], secret, sizeof(secret)) != 0) {
            printf("ERROR: secret must be 64 hex chars\n");
            return 1;
        }
        esp_err_t err = passport_store_provision(argv[2], secret);
        memset(secret, 0, sizeof(secret));
        if (err != ESP_OK) {
            printf("ERROR: provision failed: %s\n", esp_err_to_name(err));
            return 1;
        }
        printf("OK pid=%s\n", argv[2]);   // 只回显 pid
        return 0;
    }

    if (strcmp(argv[1], "wifi") == 0) {
        if (argc != 6 || strcmp(argv[2], "set") != 0 ||
            (argv[3][0] != '1' && argv[3][0] != '2') || argv[3][1] != '\0') {
            printf("usage: smpass wifi set <1|2> <ssid> <password>\n");
            return 1;
        }
        int idx = argv[3][0] - '1';
        const char *ssid_keys[2] = { PASSPORT_KEY_WIFI1_SSID, PASSPORT_KEY_WIFI2_SSID };
        const char *pass_keys[2] = { PASSPORT_KEY_WIFI1_PASS, PASSPORT_KEY_WIFI2_PASS };
        if (passport_store_set_string(ssid_keys[idx], argv[4]) != ESP_OK ||
            passport_store_set_string(pass_keys[idx], argv[5]) != ESP_OK) {
            printf("ERROR: wifi save failed\n");
            return 1;
        }
        printf("OK wifi%d ssid=%s\n", idx + 1, argv[4]);   // 不回显密码
        return 0;
    }

    if (strcmp(argv[1], "status") == 0) {
        char pid[32] = "--";
        char ssid1[33] = "--", ssid2[33] = "--", lab[33] = PASSPORT_DEFAULT_LAB_SSID;
        char stamp[24] = "--";
        int32_t balance = 0;
        uint8_t in_lab = 0, time_valid = 0;
        passport_store_get_string(PASSPORT_KEY_PID, pid, sizeof(pid));
        passport_store_get_string(PASSPORT_KEY_WIFI1_SSID, ssid1, sizeof(ssid1));
        passport_store_get_string(PASSPORT_KEY_WIFI2_SSID, ssid2, sizeof(ssid2));
        passport_store_get_string(PASSPORT_KEY_LAB_SSID, lab, sizeof(lab));
        passport_store_get_string(PASSPORT_KEY_BALANCE_TS, stamp, sizeof(stamp));
        passport_store_get_i32(PASSPORT_KEY_BALANCE, &balance);
        passport_store_get_u8(PASSPORT_KEY_IN_LAB, &in_lab);
        passport_store_get_u8(PASSPORT_KEY_TIME_VALID, &time_valid);
        printf("provisioned=%d pid=%s\n", passport_store_is_provisioned() ? 1 : 0, pid);
        printf("wifi1=%s wifi2=%s lab_ssid=%s\n", ssid1, ssid2, lab);
        printf("time_valid=%d in_lab=%d balance=%ld last_sync=%s\n",
               time_valid, in_lab, (long)balance, stamp);
        printf("version=%s\n", esp_app_get_description()->version);
        return 0;
    }

    if (strcmp(argv[1], "erase") == 0) {
        if (passport_store_erase_all() != ESP_OK) {
            printf("ERROR: erase failed\n");
            return 1;
        }
        printf("OK erased\n");
        return 0;
    }

    printf("unknown subcommand: %s\n", argv[1]);
    return 1;
}

static void console_start(void)
{
    esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    repl_config.prompt = "smpass> ";
    repl_config.task_stack_size = 4096;

    esp_console_dev_usb_serial_jtag_config_t hw_config =
        ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    esp_err_t err = esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "控制台 REPL 创建失败: %s（产线命令不可用）", esp_err_to_name(err));
        return;
    }

    const esp_console_cmd_t cmd = {
        .command = "smpass",
        .help = "SparkMinds Passport provisioning: provision/wifi/status/erase",
        .hint = NULL,
        .func = &cmd_smpass,
        .argtable = NULL,
    };
    esp_console_cmd_register(&cmd);
    esp_console_register_help_command();
    esp_console_start_repl(repl);
}

// ---------------------------------------------------------------------------
// 入口
// ---------------------------------------------------------------------------
void passport_app_start(void)
{
    if (!passport_store_is_provisioned()) {
        ESP_LOGW(TAG, "设备未激活：请经 USB 控制台执行 smpass provision");
    }

    s_key_queue = xQueueCreate(KEY_QUEUE_LEN, sizeof(key_evt_t));
    s_ui_queue = xQueueCreate(UI_QUEUE_LEN, sizeof(passport_ui_evt_t));

    passport_net_init(s_ui_queue);

    if (bsp_lvgl_lock(1000)) {
        passport_ui_init(s_ui_queue);
        passport_ui_show(PASSPORT_SCREEN_MENU);
        bsp_lvgl_unlock();
    }

    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败");
    }

    s_last_activity_us = esp_timer_get_time();
    s_last_heartbeat_s = esp_timer_get_time() / 1000000;   // 首个心跳在 10 分钟后
    s_last_sync_s = 0;

    xTaskCreate(power_task, "smpass_power", 3072, NULL, 4, &s_power_task);
    if (bsp_lvgl_lock(1000)) {
        lv_timer_create(dispatch_cb, 100, NULL);   // 属 LVGL 上下文
        bsp_lvgl_unlock();
    }

    console_start();
    ESP_LOGI(TAG, "创智学员 Passport 就绪，版本 %s", esp_app_get_description()->version);
}
