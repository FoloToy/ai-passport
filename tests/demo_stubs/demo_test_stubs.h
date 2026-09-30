#pragma once

// Minimal typed platform boundary for executing the actual demo C lifecycle code.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_TIMEOUT 0x107
const char *esp_err_to_name(esp_err_t error);
void test_log(const char *tag, const char *format, ...);
#define ESP_LOGE(...) test_log(__VA_ARGS__)
#define ESP_LOGW(...) test_log(__VA_ARGS__)
#define ESP_LOGI(...) test_log(__VA_ARGS__)

typedef int BaseType_t;
typedef unsigned TickType_t;
typedef struct test_task *TaskHandle_t;
typedef struct test_sem *SemaphoreHandle_t;
typedef void (*TaskFunction_t)(void *);
#define pdTRUE 1
#define pdPASS 1
#define pdFALSE 0
#define portMAX_DELAY UINT32_MAX
#define pdMS_TO_TICKS(ms) (ms)
#define configMAX_PRIORITIES 25
#define NIMBLE_HS_STACK_SIZE 4096
#define NIMBLE_CORE 0
#define eSetValueWithOverwrite 1
BaseType_t xTaskCreate(TaskFunction_t entry, const char *name, unsigned stack,
                       void *arg, unsigned priority, TaskHandle_t *task);
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t entry, const char *name,
                                 unsigned stack, void *arg, unsigned priority,
                                 TaskHandle_t *task, int core);
BaseType_t xTaskNotify(TaskHandle_t task, uint32_t value, int action);
BaseType_t xTaskNotifyWait(uint32_t clear_entry, uint32_t clear_exit,
                          uint32_t *value, TickType_t ticks);
void vTaskSuspend(TaskHandle_t task);
void vTaskDelete(TaskHandle_t task);
void vTaskDelay(TickType_t ticks);
SemaphoreHandle_t xSemaphoreCreateBinary(void);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks);
void vSemaphoreDelete(SemaphoreHandle_t sem);

typedef enum { BSP_BTN_UP, BSP_BTN_DOWN, BSP_BTN_OK } bsp_btn_t;
typedef enum { BSP_BTN_PRESS, BSP_BTN_CLICK, BSP_BTN_DOUBLE, BSP_BTN_LONG, BSP_BTN_RELEASE } bsp_btn_ev_t;
int bsp_button_read_mv(void);
esp_err_t bsp_button_prepare_deep_sleep(int *level);
esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t channels);
void bsp_audio_set_volume(uint8_t percent);
esp_err_t bsp_audio_read(void *pcm, size_t bytes);
esp_err_t bsp_audio_write(const void *pcm, size_t bytes);
esp_err_t bsp_audio_sleep(void);
esp_err_t bsp_audio_wake(void);
esp_err_t bsp_audio_prepare_deep_sleep(void);
esp_err_t bsp_battery_sleep(void);
esp_err_t bsp_i2c_prepare_deep_sleep(void);
esp_err_t bsp_display_prepare_deep_sleep(void);
void bsp_display_backlight(uint8_t percent);
bool bsp_lvgl_lock(int timeout);
void bsp_lvgl_unlock(void);

#define RTC_DATA_ATTR
#define ESP_SLEEP_WAKEUP_TIMER 4
#define ESP_SLEEP_WAKEUP_GPIO  7
typedef enum { ESP_GPIO_WAKEUP_GPIO_LOW = 0, ESP_GPIO_WAKEUP_GPIO_HIGH } esp_deepsleep_gpio_wake_up_mode_t;
esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t gpio_pin_mask,
                                           esp_deepsleep_gpio_wake_up_mode_t mode);
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t us);
esp_err_t esp_sleep_disable_wakeup_source(int source);
esp_err_t esp_light_sleep_start(void);
int esp_sleep_get_wakeup_cause(void);
void esp_deep_sleep_start(void);
void esp_restart(void);
int64_t esp_timer_get_time(void);

typedef struct { int unused; } lv_obj_t;
typedef struct { int unused; } lv_timer_t;
typedef struct { int unused; } lv_font_t;
extern const lv_font_t lv_font_montserrat_14;
#define LV_RADIUS_CIRCLE 1000
#define LV_TEXT_ALIGN_CENTER 0
#define LV_LABEL_LONG_WRAP 0
#define LV_ALIGN_BOTTOM_MID 0
#define LV_ALIGN_TOP_MID 1
#define LV_ALIGN_TOP_LEFT 2
uint32_t lv_color_hex(uint32_t color);
lv_obj_t *lv_obj_create(lv_obj_t *parent);
lv_obj_t *lv_label_create(lv_obj_t *parent);
void lv_obj_set_size(lv_obj_t *object, int width, int height);
void lv_obj_set_width(lv_obj_t *object, int width);
void lv_obj_set_style_radius(lv_obj_t *object, int radius, int selector);
void lv_obj_set_style_bg_color(lv_obj_t *object, uint32_t color, int selector);
void lv_obj_set_style_border_width(lv_obj_t *object, int width, int selector);
void lv_obj_set_style_text_color(lv_obj_t *object, uint32_t color, int selector);
void lv_obj_set_style_text_align(lv_obj_t *object, int align, int selector);
void lv_obj_set_style_text_font(lv_obj_t *object, const lv_font_t *font, int selector);
void lv_obj_center(lv_obj_t *object);
void lv_obj_align(lv_obj_t *object, int align, int x, int y);
void lv_obj_delete(lv_obj_t *object);
void lv_label_set_text(lv_obj_t *object, const char *text);
void lv_label_set_text_fmt(lv_obj_t *object, const char *format, ...);
void lv_label_set_long_mode(lv_obj_t *object, int mode);
void lv_screen_load(lv_obj_t *screen);
lv_timer_t *lv_timer_create(void (*callback)(lv_timer_t *), unsigned period, void *data);
void lv_timer_delete(lv_timer_t *timer);

typedef struct { int unused; } esp_netif_t;
typedef struct { int unused; } esp_netif_config_t;
#define ESP_NETIF_DEFAULT_WIFI_STA() { 0 }
typedef const char *esp_event_base_t;
typedef void *esp_event_handler_instance_t;
#define WIFI_EVENT "wifi"
#define WIFI_EVENT_SCAN_DONE 1
#define WIFI_STORAGE_RAM 0
#define WIFI_MODE_STA 0
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() { 0 }
typedef struct { int rssi; uint8_t ssid[33]; uint8_t primary; } wifi_ap_record_t;
esp_netif_t *esp_netif_new(const esp_netif_config_t *cfg);
esp_err_t esp_netif_attach_wifi_station(esp_netif_t *netif);
esp_err_t esp_wifi_set_default_wifi_sta_handlers(void);
void esp_netif_destroy_default_wifi(void *netif);
esp_err_t esp_wifi_init(const wifi_init_config_t *cfg);
esp_err_t esp_wifi_deinit(void);
esp_err_t esp_wifi_set_storage(int storage);
esp_err_t esp_wifi_set_mode(int mode);
esp_err_t esp_wifi_start(void);
esp_err_t esp_wifi_stop(void);
esp_err_t esp_wifi_scan_start(const void *cfg, bool blocking);
esp_err_t esp_wifi_scan_stop(void);
esp_err_t esp_wifi_scan_get_ap_num(uint16_t *count);
esp_err_t esp_wifi_scan_get_ap_records(uint16_t *count, wifi_ap_record_t *records);
esp_err_t esp_event_handler_instance_register(esp_event_base_t base, int32_t id,
    void (*callback)(void *, esp_event_base_t, int32_t, void *), void *arg,
    esp_event_handler_instance_t *instance);
esp_err_t esp_event_handler_instance_unregister(esp_event_base_t base, int32_t id,
    esp_event_handler_instance_t instance);

struct ble_gap_event { int type; };
struct ble_hs_adv_fields { int flags; const uint8_t *name; size_t name_len; int name_is_complete; };
struct ble_gap_adv_params { int conn_mode; int disc_mode; };
struct test_ble_hs_cfg { void (*reset_cb)(int); void (*sync_cb)(void); };
extern struct test_ble_hs_cfg ble_hs_cfg;
#define BLE_HS_ADV_F_DISC_GEN 1
#define BLE_HS_ADV_F_BREDR_UNSUP 2
#define BLE_GAP_CONN_MODE_NON 0
#define BLE_GAP_DISC_MODE_GEN 0
#define BLE_HS_FOREVER -1
#define BLE_GAP_EVENT_ADV_COMPLETE 1
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields *fields);
int ble_gap_adv_start(uint8_t address_type, const void *address, int duration,
    const struct ble_gap_adv_params *params,
    int (*callback)(struct ble_gap_event *, void *), void *arg);
int ble_gap_adv_stop(void);
int ble_hs_util_ensure_addr(int privacy);
int ble_hs_id_infer_auto(int privacy, uint8_t *type);
void ble_svc_gap_init(void);
void ble_svc_gatt_init(void);
int ble_svc_gap_device_name_set(const char *name);
esp_err_t nimble_port_init(void);
esp_err_t nimble_port_deinit(void);
int nimble_port_stop(void);
void nimble_port_run(void);
