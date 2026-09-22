// The Last Room application bootstrap and serialized input/game loop.
#include "bsp_battery.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "game_input.h"
#include "game_model.h"
#include "game_ui.h"

#include <stdbool.h>
#include <stdint.h>

#define INPUT_QUEUE_DEPTH 16
#define GAME_TICK_MS 50
#define BATTERY_REFRESH_MS 30000

static const char *TAG = "last_room";

static game_model_t s_model;
static QueueHandle_t s_input_queue;
static TaskHandle_t s_game_task;
static volatile bool s_input_ready;
static bool s_battery_available;
static int s_battery_soc = -1;

static uint64_t now_ms(void)
{
    return (uint64_t)esp_timer_get_time() / 1000ULL;
}

static game_action_t map_bsp_input(bsp_btn_t button, bsp_btn_ev_t event)
{
    game_button_t game_button;
    game_button_event_t game_event;

    switch (button) {
        case BSP_BTN_UP:
            game_button = GAME_BUTTON_UP;
            break;
        case BSP_BTN_DOWN:
            game_button = GAME_BUTTON_DOWN;
            break;
        case BSP_BTN_OK:
            game_button = GAME_BUTTON_OK;
            break;
        default:
            return GAME_ACTION_NONE;
    }

    switch (event) {
        case BSP_BTN_PRESS:
            game_event = GAME_BUTTON_EVENT_PRESS;
            break;
        case BSP_BTN_CLICK:
            game_event = GAME_BUTTON_EVENT_CLICK;
            break;
        case BSP_BTN_DOUBLE:
            game_event = GAME_BUTTON_EVENT_DOUBLE;
            break;
        case BSP_BTN_LONG:
            game_event = GAME_BUTTON_EVENT_LONG;
            break;
        default:
            return GAME_ACTION_NONE;
    }
    return game_input_map(game_button, game_event);
}

// BSP callbacks run in the shared esp_timer task. Only enqueue bounded work.
static void on_button(bsp_btn_t button, bsp_btn_ev_t event, void *user)
{
    (void)user;
    if (!s_input_ready || !s_input_queue) return;
    game_action_t action = map_bsp_input(button, event);
    if (action == GAME_ACTION_NONE) return;
    if (xQueueSend(s_input_queue, &action, 0) != pdTRUE) {
        ESP_LOGW(TAG, "input queue full: button=%d event=%d", button, event);
    }
}

static void render_if_dirty(bool *dirty)
{
    if (!*dirty || !bsp_lvgl_lock(250)) return;
    game_ui_render(&s_model, s_battery_soc);
    bsp_lvgl_unlock();
    *dirty = false;
}

static void game_task(void *arg)
{
    (void)arg;
    bool dirty = true;
    uint64_t next_battery_ms = now_ms() + BATTERY_REFRESH_MS;

    for (;;) {
        game_action_t action;
        if (xQueueReceive(s_input_queue, &action, pdMS_TO_TICKS(GAME_TICK_MS)) == pdTRUE) {
            game_model_handle_action(&s_model, action, now_ms());
            dirty = true;
        }

        uint32_t previous_remaining = s_model.remaining_time;
        uint8_t timer_events = game_model_tick(&s_model, now_ms());
        if (timer_events != GAME_TIMER_EVENT_NONE ||
            previous_remaining != s_model.remaining_time) {
            dirty = true;
        }

        uint64_t time_ms = now_ms();
        if (s_battery_available && time_ms >= next_battery_ms) {
            int soc = bsp_battery_soc();
            if (soc != s_battery_soc) {
                s_battery_soc = soc;
                dirty = true;
            }
            next_battery_ms = time_ms + BATTERY_REFRESH_MS;
        }
        render_if_dirty(&dirty);
    }
}

static void show_startup_error(const char *message)
{
    if (!bsp_lvgl_lock(500)) return;
    game_ui_show_error(message);
    bsp_lvgl_unlock();
}

void app_main(void)
{
    ESP_LOGI(TAG, "The Last Room starting (offline)");

    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "display/LVGL initialization failed; SPI MOSI=%d SCLK=%d CS=%d DC=%d BL=%d",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    game_model_init(&s_model);
    if (!bsp_lvgl_lock(1000)) {
        ESP_LOGE(TAG, "cannot acquire LVGL lock for application screen");
        return;
    }
    game_ui_create();
    game_ui_render(&s_model, s_battery_soc);
    game_model_boot_complete(&s_model, now_ms());
    game_ui_render(&s_model, s_battery_soc);
    bsp_lvgl_unlock();

    s_input_queue = xQueueCreate(INPUT_QUEUE_DEPTH, sizeof(game_action_t));
    if (!s_input_queue) {
        ESP_LOGE(TAG, "input queue allocation failed");
        show_startup_error("INPUT QUEUE FAILED");
        return;
    }

    esp_err_t button_error = bsp_button_init(on_button, NULL);
    if (button_error != ESP_OK) {
        ESP_LOGE(TAG, "button initialization failed: %s", esp_err_to_name(button_error));
        show_startup_error("BUTTONS UNAVAILABLE");
        return;
    }

    esp_err_t battery_error = bsp_battery_init();
    s_battery_available = battery_error == ESP_OK;
    if (s_battery_available) {
        s_battery_soc = bsp_battery_soc();
    } else {
        ESP_LOGW(TAG, "battery status unavailable: %s", esp_err_to_name(battery_error));
    }

    if (xTaskCreate(game_task, "last_room", 4096, NULL, 5, &s_game_task) != pdPASS) {
        ESP_LOGE(TAG, "game task allocation failed");
        show_startup_error("GAME TASK FAILED");
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
        return;
    }
    s_input_ready = true;
    ESP_LOGI(TAG, "ready: buttons=1 battery=%d", s_battery_available);
}
