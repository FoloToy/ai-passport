// main/demo_low_power.c —— light/deep sleep + 唤醒源验证。
// 两种模式入睡前均 suspend ES8311；light sleep 返回后显式恢复。
// deep sleep 还会按 CW2017 -> ES8311 -> I2S -> 共享 I2C -> LCD 顺序停止外设。
//
// deep sleep 同时武装两个唤醒源:RTC 定时器(5 秒)和三键共用的 GPIO0 低电平——
// 任一键都把该脚拉到低,所以按键、或者等 5 秒,都会唤醒并重启应用。
// 引脚交回数字输入与上拉由 bsp_button_prepare_deep_sleep() 负责:ADC 接管时该脚的
// 【数字】电平读回是 0,不恢复的话低电平唤醒条件在入睡瞬间就成立,设备会立刻醒回来。
#include "demo.h"
#include "bsp_audio.h"
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "ui_pixel.h"

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <stdio.h>
#include <stdint.h>

static const char *TAG = "demo_power";

#define LIGHT_SLEEP_TIME_US (2ULL * 1000ULL * 1000ULL)
#define DEEP_SLEEP_TIME_US  (5ULL * 1000ULL * 1000ULL)
#define DEEP_SLEEP_MAGIC    0x464F4C4FUL
#define SLEEP_STOP_TIMEOUT_MS 3000

typedef enum {
    SLEEP_COMMAND_LIGHT = 1,
    SLEEP_COMMAND_DEEP,
    SLEEP_COMMAND_STOP,
} sleep_command_t;

static lv_obj_t *s_scr;
static lv_obj_t *s_status;
static lv_obj_t *s_mode_cards[2];
static lv_obj_t *s_mascot;
static TaskHandle_t s_task;
static SemaphoreHandle_t s_stopped;
static volatile bool s_busy;
static volatile bool s_stop_requested;
static int s_selected;
static RTC_DATA_ATTR uint32_t s_deep_sleep_magic;
static RTC_DATA_ATTR uint32_t s_deep_sleep_count;

static void log_deep_sleep_warning(const char *step, esp_err_t error)
{
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "deep sleep 继续：%s 失败: %s", step,
                 esp_err_to_name(error));
    }
}

static void menu_refresh(void)
{
    for (int i = 0; i < 2; i++) {
        ui_pixel_set_selected(s_mode_cards[i], i == s_selected, true);
    }
}

static void set_status(const char *text)
{
    if (!bsp_lvgl_lock(500)) return;
    if (s_status) lv_label_set_text(s_status, text);
    bsp_lvgl_unlock();
}

static void sleep_task(void *arg)
{
    (void)arg;
    for (;;) {
        uint32_t command = 0;
        xTaskNotifyWait(0, UINT32_MAX, &command, portMAX_DELAY);
        if (command == SLEEP_COMMAND_STOP || s_stop_requested) break;

        s_busy = true;
        if (command == SLEEP_COMMAND_DEEP) {
            // 三键共用的 GPIO0 同时是深睡唤醒脚。按键被按住时低电平唤醒条件在
            // 入睡瞬间就成立,设备会立刻醒回来——所以先在按键还活着时拒绝入睡。
            int mv = bsp_button_read_mv();
            if (mv >= 0 && mv < BSP_BTN_MV_RELEASED_MIN) {
                ESP_LOGW(TAG, "按键被按住(%d mV),拒绝入睡", mv);
                set_status("KEY IS HELD\nRELEASE, THEN RUN AGAIN");
                s_busy = false;
                continue;
            }

            set_status("DEEP SLEEP: ANY KEY OR 5 SEC\nApplication will restart");
            vTaskDelay(pdMS_TO_TICKS(250));
            if (s_stop_requested) {
                s_busy = false;
                break;
            }
            esp_err_t err = esp_sleep_enable_timer_wakeup(DEEP_SLEEP_TIME_US);
            if (err == ESP_OK) {
                // 把按键脚交回普通数字输入 + 上拉,并回读电平。调用后按键在本次
                // 运行中不再可用,所以这一步之后只能继续睡。
                int level = 0;
                log_deep_sleep_warning("button pad hand-off",
                                       bsp_button_prepare_deep_sleep(&level));
                if (level != 1) {
                    // 上面已按电压挡过一次,这里再被按住就是那 250ms 的竞态;
                    // 唤醒源已无法可靠武装,继续睡下去让它立刻醒回来即可。
                    ESP_LOGW(TAG, "入睡前按键仍被按住,这次深睡可能立即返回");
                }
                // 任一键把该脚拉到低电平,故按低电平武装。第一个参数是【位掩码】,
                // 不是引脚号。
                log_deep_sleep_warning("GPIO wake arm",
                                       esp_deep_sleep_enable_gpio_wakeup(
                                           1ULL << BSP_BTN_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW));

                // CW2017 与 ES8311 共用 I2C，必须先完成电量计写入/回读。
                log_deep_sleep_warning("CW2017 suspend", bsp_battery_sleep());
                log_deep_sleep_warning("ES8311 suspend", bsp_audio_sleep());
                // 即使 codec 寄存器操作失败，也继续停时钟并释放引脚。
                log_deep_sleep_warning("I2S pin release",
                                       bsp_audio_prepare_deep_sleep());
                log_deep_sleep_warning("shared I2C pin release",
                                       bsp_i2c_prepare_deep_sleep());

                // Wi-Fi/BLE 只由各自 demo 页持有；进入本页前已经停止并释放。
                // 加锁等待当前 flush 完成，然后阻止 LVGL 在 LCD 关闭后再刷屏。
                if (!bsp_lvgl_lock(1000)) {
                    ESP_LOGE(TAG, "deep sleep 前无法停止 LVGL 刷屏，重启恢复外设");
                    esp_restart();
                }
                log_deep_sleep_warning("ST7789 suspend",
                                       bsp_display_prepare_deep_sleep());

                if (s_deep_sleep_magic != DEEP_SLEEP_MAGIC) s_deep_sleep_count = 0;
                s_deep_sleep_magic = DEEP_SLEEP_MAGIC;
                s_deep_sleep_count++;
                esp_deep_sleep_start();
                // 从 deep-sleep 准备接口返回后总线已不可在本次运行中恢复。
                ESP_LOGE(TAG, "esp_deep_sleep_start 意外返回，重启恢复外设");
                esp_restart();
            }
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            char text[96];
            snprintf(text, sizeof(text), "Deep sleep failed:\n%s", esp_err_to_name(err));
            set_status(text);
            ESP_LOGE(TAG, "Deep sleep 失败: %s", esp_err_to_name(err));
        } else {
            const char *failure = "Light sleep";
            bool audio_suspend_attempted = false;
            set_status("LIGHT SLEEP: 2 SEC\nTimer wakeup");
            vTaskDelay(pdMS_TO_TICKS(150));
            if (s_stop_requested) {
                s_busy = false;
                break;
            }
            esp_err_t err = esp_sleep_enable_timer_wakeup(LIGHT_SLEEP_TIME_US);
            if (err == ESP_OK) {
                audio_suspend_attempted = true;
                err = bsp_audio_sleep();
                failure = "Audio suspend";
            }
            if (err == ESP_OK) bsp_display_backlight(0);
            int64_t before = esp_timer_get_time();
            if (err == ESP_OK) {
                failure = "Light sleep";
                err = esp_light_sleep_start();
            }
            int64_t slept_ms = (esp_timer_get_time() - before) / 1000;
            esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
            // bsp_audio_sleep() 可能在已停 I2S 后报寄存器校验失败，
            // 因此只要尝试过 suspend，未进入 light sleep 也必须恢复。
            if (audio_suspend_attempted) {
                esp_err_t wake_err = bsp_audio_wake();
                if (err == ESP_OK && wake_err != ESP_OK) {
                    err = wake_err;
                    failure = "Audio resume";
                }
            }
            bsp_display_backlight(100);

            char text[128];
            if (err == ESP_OK) {
                snprintf(text, sizeof(text), "LIGHT WAKE: TIMER\nSlept: %lld ms",
                         (long long)slept_ms);
            } else {
                snprintf(text, sizeof(text), "%s failed:\n%s", failure, esp_err_to_name(err));
                ESP_LOGE(TAG, "%s 失败: %s", failure, esp_err_to_name(err));
            }
            set_status(text);
        }
        s_busy = false;
    }
    s_busy = false;
    // Acknowledge only after all shared-state/UI work. The lifecycle owner deletes
    // the task, so retries never notify a stale handle and re-entry cannot race an
    // old worker clearing the new worker's handle.
    xSemaphoreGive(s_stopped);
    for (;;) vTaskSuspend(NULL);
}

void demo_low_power_enter(void)
{
    s_scr = ui_pixel_screen_create("LOW POWER");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 14, 54, 212, 190, UI_PAPER);
    s_status = lv_label_create(panel);
    lv_obj_set_width(s_status, 184);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_status, lv_color_hex(UI_INK), 0);
    lv_obj_align(s_status, LV_ALIGN_TOP_MID, 0, 1);
    if (s_deep_sleep_magic == DEEP_SLEEP_MAGIC &&
        esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO) {
        lv_label_set_text_fmt(s_status,
                              "DEEP KEY WAKE  #%lu\nUP/DOWN: SELECT  OK: RUN",
                              (unsigned long)s_deep_sleep_count);
    } else if (s_deep_sleep_magic == DEEP_SLEEP_MAGIC &&
               esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
        lv_label_set_text_fmt(s_status,
                              "DEEP TIMER WAKE  #%lu\nUP/DOWN: SELECT  OK: RUN",
                              (unsigned long)s_deep_sleep_count);
    } else {
        lv_label_set_text(s_status, "UP/DOWN: SELECT  OK: RUN\nANY KEY OR 5 SEC WAKES");
    }

    static const char *MODE_NAMES[] = {
        "LIGHT SLEEP  |  2 SEC",
        "DEEP SLEEP   |  5 SEC",
    };
    for (int i = 0; i < 2; i++) {
        s_mode_cards[i] = ui_pixel_panel_create(panel, 7, 56 + i * 54,
                                                 176, 42, UI_PAPER);
        lv_obj_t *label = lv_label_create(s_mode_cards[i]);
        lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(label, lv_color_hex(UI_INK), 0);
        lv_label_set_text(label, MODE_NAMES[i]);
        lv_obj_center(label);
    }
    s_selected = 0;
    menu_refresh();
    s_mascot = ui_pixel_mascot_create(s_scr, 101, 246);
    s_busy = false;
    lv_screen_load(s_scr);
}

esp_err_t demo_low_power_start(void)
{
    if (s_task) return ESP_OK;
    if (s_stopped) {
        vSemaphoreDelete(s_stopped);
        s_stopped = NULL;
    }
    s_stopped = xSemaphoreCreateBinary();
    if (!s_stopped) {
        set_status("Cannot create\nsleep worker");
        return ESP_ERR_NO_MEM;
    }
    s_stop_requested = false;
    if (xTaskCreate(sleep_task, "demo_sleep", 3072, NULL, 4, &s_task) != pdPASS) {
        vSemaphoreDelete(s_stopped);
        s_stopped = NULL;
        set_status("Cannot create\nsleep worker");
        ESP_LOGE(TAG, "创建 light-sleep 任务失败");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t demo_low_power_stop(void)
{
    TaskHandle_t task = s_task;
    if (!task) {
        if (s_stopped) {
            vSemaphoreDelete(s_stopped);
            s_stopped = NULL;
        }
        return ESP_OK;
    }

    s_stop_requested = true;
    xTaskNotify(task, SLEEP_COMMAND_STOP, eSetValueWithOverwrite);
    if (!s_stopped ||
        xSemaphoreTake(s_stopped, pdMS_TO_TICKS(SLEEP_STOP_TIMEOUT_MS)) != pdTRUE) {
        set_status("Sleep stop timed out; retry");
        return ESP_ERR_TIMEOUT;
    }
    vTaskDelete(task);
    s_task = NULL;
    vSemaphoreDelete(s_stopped);
    s_stopped = NULL;
    bsp_display_backlight(100);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    return ESP_OK;
}

void demo_low_power_exit(void)
{
    s_busy = false;
    bsp_display_backlight(100);
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_status = NULL;
        s_mode_cards[0] = s_mode_cards[1] = NULL;
        s_mascot = NULL;
    }
}

void demo_low_power_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_CLICK || s_busy || s_stop_requested || !s_task) return;
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        if (!bsp_lvgl_lock(250)) return;
        s_selected = (s_selected + 1) % 2;
        menu_refresh();
        ui_pixel_mascot_jump(s_mascot);
        bsp_lvgl_unlock();
    } else if (btn == BSP_BTN_OK) {
        uint32_t command = s_selected == 0 ? SLEEP_COMMAND_LIGHT : SLEEP_COMMAND_DEEP;
        xTaskNotify(s_task, command, eSetValueWithOverwrite);
    }
}
