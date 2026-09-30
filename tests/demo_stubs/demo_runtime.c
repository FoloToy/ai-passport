// Deterministic RTOS boundary: workers execute until they park at vTaskSuspend.
// Tests control acknowledgement timing; notifying or deleting stale handles fails.
#include "demo_test_stubs.h"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

struct test_task { TaskFunction_t entry; void *arg; bool deleted; uint32_t notification; };
struct test_sem { bool available; bool deleted; };
struct test_task test_tasks[16];
struct test_sem test_sems[16];
unsigned test_task_count, test_sem_count, test_task_deletes, test_notifications;
bool test_create_fails, test_sem_fails, test_auto_ack;
TaskHandle_t test_current_task, test_latest_task;
jmp_buf test_worker_park;
char test_status[160];
unsigned test_read_calls, test_write_calls, test_read_fail_at, test_write_fail_at;
void (*test_read_hook)(void);
struct test_ble_hs_cfg ble_hs_cfg;
int test_nimble_deinit_result, test_nimble_stop_result, test_nimble_name_result;
unsigned test_nimble_deinits, test_nimble_stops;
const lv_font_t lv_font_montserrat_14 = { 0 };

void test_log(const char *tag, const char *format, ...) { (void)tag; (void)format; }
const char *esp_err_to_name(esp_err_t error) { (void)error; return "test error"; }
bool bsp_lvgl_lock(int timeout) { (void)timeout; return true; }
void bsp_lvgl_unlock(void) {}
void lv_label_set_text(lv_obj_t *object, const char *text) {
    (void)object;
    snprintf(test_status, sizeof(test_status), "%s", text);
}
void bsp_display_backlight(uint8_t percent) { (void)percent; }
void ui_pixel_mascot_jump(lv_obj_t *mascot) { (void)mascot; }
void ui_pixel_set_selected(lv_obj_t *panel, bool selected, bool enabled) {
    (void)panel; (void)selected; (void)enabled;
}
esp_err_t esp_sleep_disable_wakeup_source(int source) { (void)source; return ESP_OK; }
esp_err_t esp_sleep_enable_timer_wakeup(uint64_t us) { (void)us; return ESP_OK; }
esp_err_t esp_light_sleep_start(void) { return ESP_OK; }
void esp_deep_sleep_start(void) { assert(!"unexpected deep sleep in host test"); }
void esp_restart(void) { assert(!"unexpected restart in host test"); }
int64_t esp_timer_get_time(void) { return 0; }
esp_err_t bsp_audio_sleep(void) { return ESP_OK; }
esp_err_t bsp_audio_wake(void) { return ESP_OK; }
esp_err_t bsp_audio_prepare_deep_sleep(void) { return ESP_OK; }
esp_err_t bsp_battery_sleep(void) { return ESP_OK; }

// Key pad: tests set test_btn_mv to the voltage a held key would produce
// (see the windows in bsp_pins.h), or leave it released.
int test_btn_mv = 3300;
int bsp_button_read_mv(void) { return test_btn_mv; }
esp_err_t bsp_button_prepare_deep_sleep(int *level) {
    if (level) *level = 1;
    return ESP_OK;
}
esp_err_t esp_deep_sleep_enable_gpio_wakeup(uint64_t gpio_pin_mask,
                                           esp_deepsleep_gpio_wake_up_mode_t mode) {
    (void)gpio_pin_mask; (void)mode;
    return ESP_OK;
}
esp_err_t bsp_i2c_prepare_deep_sleep(void) { return ESP_OK; }
esp_err_t bsp_display_prepare_deep_sleep(void) { return ESP_OK; }
esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t channels) {
    (void)hz; (void)bits; (void)channels; return ESP_OK;
}
void bsp_audio_set_volume(uint8_t percent) { (void)percent; }
esp_err_t bsp_audio_read(void *pcm, size_t bytes) {
    test_read_calls++;
    if (test_read_hook) test_read_hook();
    if (test_read_calls == test_read_fail_at) return ESP_FAIL;
    memset(pcm, 0, bytes);
    return ESP_OK;
}
esp_err_t bsp_audio_write(const void *pcm, size_t bytes) {
    (void)pcm; (void)bytes;
    return ++test_write_calls == test_write_fail_at ? ESP_FAIL : ESP_OK;
}

BaseType_t xTaskCreate(TaskFunction_t entry, const char *name, unsigned stack,
                       void *arg, unsigned priority, TaskHandle_t *task) {
    (void)name; (void)stack; (void)priority;
    if (test_create_fails) return pdFALSE;
    assert(test_task_count < 16);
    *task = &test_tasks[test_task_count++];
    **task = (struct test_task){ .entry = entry, .arg = arg };
    test_latest_task = *task;
    return pdPASS;
}
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t entry, const char *name,
                                 unsigned stack, void *arg, unsigned priority,
                                 TaskHandle_t *task, int core) {
    (void)core;
    return xTaskCreate(entry, name, stack, arg, priority, task);
}
BaseType_t xTaskNotify(TaskHandle_t task, uint32_t value, int action) {
    (void)action;
    assert(task && !task->deleted);
    task->notification = value;
    test_notifications++;
    return pdTRUE;
}
BaseType_t xTaskNotifyWait(uint32_t clear_entry, uint32_t clear_exit,
                          uint32_t *value, TickType_t ticks) {
    (void)clear_entry; (void)clear_exit; (void)ticks;
    assert(test_current_task && test_current_task->notification);
    *value = test_current_task->notification;
    test_current_task->notification = 0;
    return pdTRUE;
}
void test_run_worker(TaskHandle_t task) {
    assert(task && !task->deleted);
    test_current_task = task;
    if (setjmp(test_worker_park) == 0) {
        task->entry(task->arg);
        assert(!"worker unexpectedly returned");
    }
    test_current_task = NULL;
}
void vTaskSuspend(TaskHandle_t task) {
    assert(task == NULL && test_current_task);
    longjmp(test_worker_park, 1);
}
void vTaskDelete(TaskHandle_t task) {
    // Workers must not delete themselves; only the owner may reap an acked task.
    assert(task && !task->deleted && !test_current_task);
    task->deleted = true;
    test_task_deletes++;
}
void vTaskDelay(TickType_t ticks) { (void)ticks; }
SemaphoreHandle_t xSemaphoreCreateBinary(void) {
    if (test_sem_fails) return NULL;
    assert(test_sem_count < 16);
    SemaphoreHandle_t sem = &test_sems[test_sem_count++];
    *sem = (struct test_sem){ 0 };
    return sem;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) {
    assert(sem && !sem->deleted);
    sem->available = true;
    return pdTRUE;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t ticks) {
    (void)ticks;
    assert(sem && !sem->deleted);
    if (!sem->available && test_auto_ack) test_run_worker(test_latest_task);
    if (!sem->available) return pdFALSE;
    sem->available = false;
    return pdTRUE;
}
void vSemaphoreDelete(SemaphoreHandle_t sem) {
    assert(sem && !sem->deleted);
    sem->deleted = true;
}

esp_err_t demo_radio_nvs_prepare(void) { return ESP_OK; }
esp_err_t demo_radio_network_prepare(void) { return ESP_OK; }
esp_err_t nimble_port_init(void) { return ESP_OK; }
esp_err_t nimble_port_deinit(void) {
    test_nimble_deinits++;
    assert(!test_current_task);
    return test_nimble_deinit_result;
}
int nimble_port_stop(void) { test_nimble_stops++; return test_nimble_stop_result; }
void nimble_port_run(void) {}
void ble_svc_gap_init(void) {}
void ble_svc_gatt_init(void) {}
int ble_svc_gap_device_name_set(const char *name) { (void)name; return test_nimble_name_result; }
int ble_hs_util_ensure_addr(int privacy) { (void)privacy; return 0; }
int ble_hs_id_infer_auto(int privacy, uint8_t *type) { (void)privacy; *type = 0; return 0; }
int ble_gap_adv_set_fields(const struct ble_hs_adv_fields *fields) { (void)fields; return 0; }
int ble_gap_adv_start(uint8_t address_type, const void *address, int duration,
    const struct ble_gap_adv_params *params,
    int (*callback)(struct ble_gap_event *, void *), void *arg) {
    (void)address_type; (void)address; (void)duration; (void)params; (void)callback; (void)arg;
    return 0;
}
int ble_gap_adv_stop(void) { return 0; }
