#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_i2c.h"
#include "bsp_pins.h"
#include "jpeg_store.h"
#include "jpeg_view.h"
#include "safety_portal.h"
#include "safety_profile.h"
#include "safety_store.h"
#include "ui_safety.h"

#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#include <stdbool.h>
#include <stdint.h>

#define NORMAL_IDLE_MS (60u * 1000u)
#define PORTAL_IDLE_MS (5u * 60u * 1000u)
#define SAVED_GRACE_MS 1600u

typedef enum {
    MODE_PROFILE = 0,
    MODE_RESET_CONFIRM,
    MODE_PORTAL,
    MODE_SAVED,
} app_mode_t;

typedef struct {
    bsp_btn_t button;
    bsp_btn_ev_t event;
} button_message_t;

static const char *TAG = "safety_app";
static safety_profile_t s_profile;
static QueueHandle_t s_button_queue;
static app_mode_t s_mode;
static int s_page;
static int s_battery_percent = -1;
static uint32_t s_last_activity_ms;
static uint32_t s_saved_at_ms;
static bool s_accept_buttons;
static volatile bool s_qr_failed;

static uint32_t now_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

static void note_activity(void) { s_last_activity_ms = now_ms(); }

static int current_battery(void)
{
    int value = bsp_battery_soc();
    if (value >= 0) s_battery_percent = value;
    return s_battery_percent;
}

static void show_profile(void)
{
    int battery = current_battery();
    bool has_qr = jpeg_store_has_valid();
    if (!bsp_lvgl_lock(1000)) return;
    ui_safety_show_profile(&s_profile, s_page, has_qr, battery);
    bsp_lvgl_unlock();
}

static void show_reset(void)
{
    int battery = current_battery();
    if (!bsp_lvgl_lock(1000)) return;
    ui_safety_show_reset_confirm(battery);
    bsp_lvgl_unlock();
}

static void show_saved(void)
{
    int battery = current_battery();
    if (!bsp_lvgl_lock(1000)) return;
    ui_safety_show_saved(battery);
    bsp_lvgl_unlock();
}

static void show_qr_loading(void)
{
    int battery = current_battery();
    if (!bsp_lvgl_lock(1000)) return;
    ui_safety_show_qr_loading(battery);
    bsp_lvgl_unlock();
}

static void show_qr_error(void)
{
    int battery = current_battery();
    if (!bsp_lvgl_lock(1000)) return;
    ui_safety_show_qr_error(battery);
    bsp_lvgl_unlock();
}

static void show_portal(void)
{
    int battery = current_battery();
    if (!bsp_lvgl_lock(1000)) return;
    ui_safety_show_setup(safety_portal_ssid(), safety_portal_password(),
                         !s_profile.configured, battery);
    bsp_lvgl_unlock();
}

static bool start_portal(void)
{
    bool first_setup = !s_profile.configured;
    if (!safety_portal_start(&s_profile, first_setup)) {
        ESP_LOGE(TAG, "Failed to start local setup AP");
        return false;
    }
    s_mode = MODE_PORTAL;
    note_activity();
    show_portal();
    return true;
}

static void button_callback(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    button_message_t message = {.button = button, .event = event};
    if (s_button_queue) xQueueSend(s_button_queue, &message, 0);
}

static void qr_result(bool success, void *user)
{
    (void)user;
    if (!success) s_qr_failed = true;
}

static void close_qr_view(void)
{
    jpeg_view_exit();
    show_profile();
}

static void handle_profile_button(const button_message_t *message)
{
    if (jpeg_view_is_busy()) return;
    if (jpeg_view_is_active()) {
        if (message->event == BSP_BTN_CLICK || message->event == BSP_BTN_LONG) {
            close_qr_view();
        }
        return;
    }
    if (message->event == BSP_BTN_LONG && message->button == BSP_BTN_OK) {
        s_mode = MODE_RESET_CONFIRM;
        show_reset();
        return;
    }
    if (message->event != BSP_BTN_CLICK) return;
    if (message->button == BSP_BTN_UP) {
        s_page = (s_page + UI_SAFETY_PAGE_COUNT - 1) % UI_SAFETY_PAGE_COUNT;
        show_profile();
    } else if (message->button == BSP_BTN_DOWN) {
        s_page = (s_page + 1) % UI_SAFETY_PAGE_COUNT;
        show_profile();
    } else if (message->button == BSP_BTN_OK &&
               s_page == UI_SAFETY_PAGE_COUNT - 1 &&
               jpeg_store_has_valid()) {
        show_qr_loading();
        if (!jpeg_view_request()) show_qr_error();
    }
}

static void handle_button(const button_message_t *message)
{
    note_activity();
    if (!s_accept_buttons) return;
    if (s_mode == MODE_PROFILE) {
        handle_profile_button(message);
    } else if (s_mode == MODE_RESET_CONFIRM) {
        if (message->event == BSP_BTN_CLICK &&
            message->button == BSP_BTN_OK) {
            if (!start_portal()) {
                s_mode = MODE_PROFILE;
                show_profile();
            }
        } else if ((message->event == BSP_BTN_CLICK &&
                    message->button != BSP_BTN_OK) ||
                   (message->event == BSP_BTN_LONG &&
                    message->button == BSP_BTN_OK)) {
            s_mode = MODE_PROFILE;
            show_profile();
        }
    } else if (s_mode == MODE_PORTAL && s_profile.configured &&
               message->event == BSP_BTN_LONG &&
               message->button == BSP_BTN_OK) {
        safety_portal_stop();
        s_mode = MODE_PROFILE;
        show_profile();
    }
}

static void enter_deep_sleep(void)
{
    if (bsp_button_any_pressed()) {
        ESP_LOGW(TAG, "A function key is still held; delaying sleep");
        note_activity();
        return;
    }
    esp_err_t error = esp_deep_sleep_enable_gpio_wakeup(
        1ULL << BSP_BTN_GPIO, ESP_GPIO_WAKEUP_GPIO_LOW);
    if (error != ESP_OK) {
        ESP_LOGE(TAG, "Cannot configure function-key wake: %s",
                 esp_err_to_name(error));
        note_activity();
        return;
    }
    if (jpeg_view_is_active()) jpeg_view_exit();
    if (safety_portal_is_running()) safety_portal_stop();
    bsp_display_backlight(0);
    error = bsp_button_deinit();
    if (error != ESP_OK) {
        ESP_LOGW(TAG, "Button shutdown before sleep returned: %s",
                 esp_err_to_name(error));
    }
    ESP_LOGI(TAG, "Entering deep sleep; any function key wakes the device");
    esp_deep_sleep_start();
}

static void process_saved_profile(void)
{
    if (s_mode != MODE_PORTAL || !safety_portal_take_saved()) return;
    s_mode = MODE_SAVED;
    s_saved_at_ms = now_ms();
    show_saved();
}

void app_main(void)
{
    bool woke_from_button =
        esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_GPIO;
    bool configured = safety_store_load(&s_profile);
    s_profile.configured = configured ? 1 : 0;

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "Display/LVGL initialization failed");
        return;
    }
    bsp_display_backlight(82);

    if (bsp_i2c_init() == ESP_OK && bsp_battery_init() == ESP_OK) {
        s_battery_percent = bsp_battery_soc();
    } else {
        ESP_LOGW(TAG, "Battery gauge unavailable; UI will omit battery level");
    }

    s_button_queue = xQueueCreate(8, sizeof(button_message_t));
    if (!s_button_queue ||
        bsp_button_init(button_callback, NULL) != ESP_OK ||
        !jpeg_view_init(qr_result, NULL)) {
        ESP_LOGE(TAG, "Button queue or QR viewer initialization failed");
        return;
    }

    s_accept_buttons = !woke_from_button;
    s_page = 0;
    note_activity();
    if (configured) {
        s_mode = MODE_PROFILE;
        show_profile();
    } else if (!start_portal()) {
        s_mode = MODE_PROFILE;
        show_profile();
    }

    for (;;) {
        if (!s_accept_buttons && !bsp_button_any_pressed()) {
            s_accept_buttons = true;
            note_activity();
        }

        button_message_t message;
        if (xQueueReceive(s_button_queue, &message,
                          pdMS_TO_TICKS(100)) == pdTRUE) {
            handle_button(&message);
        }
        process_saved_profile();
        if (s_qr_failed) {
            s_qr_failed = false;
            show_qr_error();
        }

        uint32_t now = now_ms();
        if (s_mode == MODE_SAVED && now - s_saved_at_ms >= SAVED_GRACE_MS) {
            safety_portal_stop();
            s_mode = MODE_PROFILE;
            s_page = 0;
            note_activity();
            show_profile();
        }
        uint32_t threshold = s_mode == MODE_PORTAL
                                 ? PORTAL_IDLE_MS : NORMAL_IDLE_MS;
        if (s_mode != MODE_SAVED && !jpeg_view_is_busy() &&
            now - s_last_activity_ms >= threshold) {
            enter_deep_sleep();
        }
    }
}
