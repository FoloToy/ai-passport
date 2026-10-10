// main/demo_finder.c —— Pocket Finder 应用入口（页面注册与接线）。
//
// 线程模型（重要）：
//   * 扫描回调        跑在 NimBLE host 任务 → 只入队；
//   * 按键回调        跑在主输入任务        → 只入队；
//   * 其余全部         跑在 LVGL 任务（lv_timer 回调）→ 状态机、设备表、界面、音频等级。
// 这样 finder_table / finder_ui 只被一个任务改，无需额外互斥；也让"回调只做有界工作"
// 这条运行时约束落到实处。
#include "bsp_display.h"
#include "demo.h"
#include "demo_radio.h"
#include "finder_audio.h"
#include "finder_idle.h"
#include "finder_scan.h"
#include "finder_table.h"
#include "finder_ui.h"
#include "finder_view.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "lvgl.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "demo_finder";

#define FINDER_TICK_MS        100
#define FINDER_SCAN_Q_DEPTH    32
#define FINDER_INPUT_Q_DEPTH    8
#define FINDER_DRAIN_PER_TICK  16
#define FINDER_NOTICE_MAX      40

#define FINDER_NVS_NAMESPACE "finder"
// 键名带版本：说明页要求"每个固件版本一次"，换版本就该重新看一遍。
#define FINDER_NVS_KEY_INTRO "intro_v1"

static finder_table_t s_table;
static finder_ui_t    s_ui;
static QueueHandle_t  s_scan_queue;
static QueueHandle_t  s_input_queue;
static lv_timer_t    *s_timer;
static finder_page_t  s_shown_page;
static bool           s_shown_valid;
static bool           s_intro_dismiss_pending;
static volatile bool  s_leave_requested;
static volatile bool  s_active;
static esp_err_t      s_scan_err;
static finder_idle_t  s_idle;
static uint8_t        s_backlight;

// 闲置计时用 32 位毫秒（xTaskGetTickCount() 的原生宽度，约 49 天回绕）：
// finder_idle 被按键任务写、被本文件的 tick（LVGL 任务）读，32 位对齐读写在
// ESP32-C3 上是单条指令，不会读到撕裂值。理由与回绕处理见 finder_idle.h。
static uint32_t now_ms_u32(void)
{
    return (uint32_t)xTaskGetTickCount() * (uint32_t)portTICK_PERIOD_MS;
}

// 聚焦目标用【地址】而不是槽位号跟随：槽位会被淘汰后复用给别的设备，
// 只认槽位号会在后台悄悄把用户追的设备换掉。
static uint8_t s_focus_addr[6];
static bool    s_focus_addr_valid;

static char s_notice_text[FINDER_NOTICE_MAX];

// ---- NVS：说明页"已读"标记 --------------------------------------------

static bool intro_seen(void)
{
    nvs_handle_t handle;
    if (nvs_open(FINDER_NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) {
        return false;
    }
    uint8_t value = 0;
    esp_err_t err = nvs_get_u8(handle, FINDER_NVS_KEY_INTRO, &value);
    nvs_close(handle);
    return err == ESP_OK && value != 0;
}

// 只在音频停止后调用：NVS 写入会关 cache，可能打断 I2S。
static void intro_store(void)
{
    nvs_handle_t handle;
    if (nvs_open(FINDER_NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) {
        return;
    }
    if (nvs_set_u8(handle, FINDER_NVS_KEY_INTRO, 1) == ESP_OK) {
        (void)nvs_commit(handle);
    }
    nvs_close(handle);
}

// ---- 扫描回调：只入队 ---------------------------------------------------

static void on_scan_result(const finder_scan_result_t *result, void *user)
{
    (void)user;
    if (!s_scan_queue || !result) {
        return;
    }
    // 队列满就丢弃本帧：丢一个广播样本不影响滤波收敛，阻塞 BLE host 会。
    (void)xQueueSend(s_scan_queue, result, 0);
}

// ---- 按键翻译：只入队 ---------------------------------------------------

static finder_input_t translate(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev == BSP_BTN_LONG) {
        if (btn == BSP_BTN_OK) {
            return FINDER_INPUT_OK_LONG;
        }
        if (btn == BSP_BTN_UP) {
            return FINDER_INPUT_UP_LONG;
        }
        if (btn == BSP_BTN_DOWN) {
            return FINDER_INPUT_DOWN_LONG;
        }
        return FINDER_INPUT_NONE;
    }
    if (ev == BSP_BTN_CLICK) {
        if (btn == BSP_BTN_UP) {
            return FINDER_INPUT_UP_CLICK;
        }
        if (btn == BSP_BTN_DOWN) {
            return FINDER_INPUT_DOWN_CLICK;
        }
        if (btn == BSP_BTN_OK) {
            return FINDER_INPUT_OK_CLICK;
        }
        return FINDER_INPUT_NONE;
    }
    return FINDER_INPUT_NONE; // PRESS / DOUBLE 本应用不使用
}

void demo_finder_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (!s_input_queue) {
        return;
    }
    // 息屏后的第一下只用来点亮屏幕：同一次按压后续还会产生 RELEASE / CLICK / LONG，
    // 它们在唤醒宽限期内一并被丢弃。看不见屏幕时触发动作，用户无法知道自己改了什么
    // （"短按 OK = 下一个设备"本身已是静默传送，见 PRD 风险 R10）。
    if (finder_idle_note_key(&s_idle, now_ms_u32())) {
        return;
    }
    finder_input_t input = translate(btn, ev);
    if (input == FINDER_INPUT_NONE) {
        return;
    }
    (void)xQueueSend(s_input_queue, &input, 0);
}

bool demo_finder_poll_leave(void)
{
    // 一次性：main 的收尾若失败（例如 stop 超时），不应每 50ms 重试刷屏；
    // 用户再按一次两步退出即可重来。
    if (!s_leave_requested) {
        return false;
    }
    s_leave_requested = false;
    return true;
}

// ---- 聚焦目标跟随 -------------------------------------------------------

static void focus_capture(int8_t slot)
{
    s_focus_addr_valid = false;
    if (slot >= 0 && slot < FINDER_TABLE_MAX && s_table.entry[slot].used) {
        memcpy(s_focus_addr, s_table.entry[slot].addr, 6);
        s_focus_addr_valid = true;
    }
}

static bool focus_alive(int8_t slot)
{
    if (!s_focus_addr_valid || slot < 0 || slot >= FINDER_TABLE_MAX) {
        return false;
    }
    const finder_entry_t *e = &s_table.entry[slot];
    return e->used && memcmp(e->addr, s_focus_addr, 6) == 0;
}

// ---- 提示文案 -----------------------------------------------------------

static const char *notice_text(int64_t now_ms)
{
    (void)now_ms;
    if (!finder_scan_running()) {
        // 扫描没起来时界面必须说出来，否则会像"周围什么都没有"。
        snprintf(s_notice_text, sizeof(s_notice_text), "SCAN FAILED (%d)", (int)s_scan_err);
        return s_notice_text;
    }
    switch (s_ui.notice) {
    case FINDER_NOTICE_DEVICE_GONE:
        snprintf(s_notice_text, sizeof(s_notice_text), "DEVICE GONE");
        break;
    case FINDER_NOTICE_ONLY_DEVICE:
        snprintf(s_notice_text, sizeof(s_notice_text), "ONLY DEVICE");
        break;
    case FINDER_NOTICE_NEXT: {
        int8_t slot = s_ui.focus;
        const char *name = "-";
        if (slot >= 0 && slot < FINDER_TABLE_MAX && s_table.entry[slot].used) {
            name = (s_table.entry[slot].name[0] != '\0') ? s_table.entry[slot].name : "DEVICE";
        }
        snprintf(s_notice_text, sizeof(s_notice_text), "NEXT -> %s", name);
        break;
    }
    default:
        s_notice_text[0] = '\0';
        break;
    }
    return s_notice_text;
}

// ---- 动作与刷新 ---------------------------------------------------------

static void apply_action(const finder_ui_result_t *result, int64_t now_ms)
{
    switch (result->action) {
    case FINDER_ACTION_INTRO_SEEN:
        // 真正的 NVS 写入推迟到音频停止之后（见 intro_store 的注释）。
        s_intro_dismiss_pending = true;
        break;

    case FINDER_ACTION_NEXT_DEVICE: {
        uint8_t count = finder_table_count(&s_table);
        if (count <= 1) {
            finder_ui_set_notice(&s_ui, FINDER_NOTICE_ONLY_DEVICE, now_ms);
            break;
        }
        int8_t next = finder_table_next(&s_table, now_ms, s_ui.focus);
        finder_ui_select(&s_ui, next);
        focus_capture(next);
        finder_ui_set_notice(&s_ui, FINDER_NOTICE_NEXT, now_ms);
        break;
    }

    case FINDER_ACTION_LEAVE:
        s_leave_requested = true;
        break;

    default:
        break;
    }
}

static void sync_page(void)
{
    if (!s_shown_valid || s_shown_page != s_ui.page) {
        finder_view_show(s_ui.page);
        s_shown_page = s_ui.page;
        s_shown_valid = true;
    }
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    if (!s_active) {
        return;
    }
    int64_t now_ms = (int64_t)xTaskGetTickCount() * portTICK_PERIOD_MS;

    // 0) 待机显示：闲置 1 分钟背光减半、3 分钟息屏。这里是背光的唯一写入者，
    //    且只在目标值变化时才调 LEDC，不每个 tick 重配一次。
    uint8_t want_backlight =
        finder_idle_backlight(finder_idle_state_at(&s_idle, (uint32_t)now_ms));
    if (want_backlight != s_backlight) {
        bsp_display_backlight(want_backlight);
        s_backlight = want_backlight;
        // 真机验收靠串口看这两次跳变，不必盯着屏幕掐表。
        ESP_LOGI(TAG, "背光 %u%%（闲置 %u ms）", (unsigned)want_backlight,
                 (unsigned)((uint32_t)now_ms - s_idle.last_activity_ms));
    }

    // 1) 扫描结果 → 设备表（回调只入队，实际工作在这里做）
    finder_scan_result_t sample;
    int drained = 0;
    while (drained < FINDER_DRAIN_PER_TICK &&
           xQueueReceive(s_scan_queue, &sample, 0) == pdTRUE) {
        finder_table_observe(&s_table, sample.addr,
                             sample.has_name ? sample.name : NULL,
                             sample.rssi, now_ms);
        drained++;
    }

    // 2) 老化与趋势窗口
    finder_table_refresh(&s_table, now_ms);

    // 3) 按键 → 状态机（在 LVGL 任务内处理，避免状态机被两个任务同时改）
    finder_input_t input;
    while (xQueueReceive(s_input_queue, &input, 0) == pdTRUE) {
        finder_ui_result_t result =
            finder_ui_handle(&s_ui, input, now_ms, finder_table_count(&s_table));
        apply_action(&result, now_ms);
        if (result.action == FINDER_ACTION_REDRAW && result.index != s_ui.focus) {
            focus_capture(result.index);
        }
    }

    // 4) 聚焦中的设备消失（被老化或被别的设备顶替）→ 回雷达页并提示
    if (s_ui.page == FINDER_PAGE_FOCUS && !focus_alive(s_ui.focus)) {
        finder_ui_set_page(&s_ui, FINDER_PAGE_RADAR);
        finder_ui_set_notice(&s_ui, FINDER_NOTICE_DEVICE_GONE, now_ms);
    }
    if (s_ui.page == FINDER_PAGE_FOCUS && s_ui.focus >= 0 &&
        s_ui.focus < FINDER_TABLE_MAX && s_table.entry[s_ui.focus].used) {
        focus_capture(s_ui.focus); // 槽位复用后重新锚定到当前设备
    }

    // 5) 超时撤销与提示过期
    (void)finder_ui_tick(&s_ui, now_ms);

    // 6) 蜂鸣等级：聚焦页跟聚焦目标；雷达页跟选中项；说明页静音
    int8_t beep_level = 0;
    if (s_ui.page == FINDER_PAGE_FOCUS && s_ui.focus >= 0 &&
        s_ui.focus < FINDER_TABLE_MAX && s_table.entry[s_ui.focus].used) {
        beep_level = finder_rssi_level(&s_table.entry[s_ui.focus].rssi, now_ms);
    } else if (s_ui.page == FINDER_PAGE_RADAR && s_ui.selected >= 0 &&
               s_ui.selected < FINDER_TABLE_MAX && s_table.entry[s_ui.selected].used) {
        beep_level = finder_rssi_level(&s_table.entry[s_ui.selected].rssi, now_ms);
    }
    finder_audio_set(beep_level, s_ui.muted);

    // 7) 页面切换与重绘
    sync_page();
    finder_view_update(&s_ui, &s_table, now_ms, notice_text(now_ms));
}

// ---- 页面生命周期 -------------------------------------------------------

void demo_finder_enter(void)
{
    // enter 可能在 NVS 初始化之前发生（扫描/音频在 start 里才起），
    // 但读"说明页已读"标记需要 NVS，所以这里先准备一次。
    if (demo_radio_nvs_prepare() != ESP_OK) {
        ESP_LOGW(TAG, "NVS 不可用：说明页标记无法读取或保存");
    }

    finder_table_reset(&s_table);
    finder_ui_init(&s_ui, intro_seen());

    s_shown_valid = false;
    s_focus_addr_valid = false;
    s_leave_requested = false;
    s_intro_dismiss_pending = false;
    s_notice_text[0] = '\0';
    // 从"刚刚有活动"起算；同时先恢复全亮——上一个应用可能把背光留成 0。
    finder_idle_init(&s_idle, now_ms_u32());
    bsp_display_backlight((uint8_t)FINDER_IDLE_FULL_PERCENT);
    s_backlight = (uint8_t)FINDER_IDLE_FULL_PERCENT;
    s_active = true;

    sync_page();
    s_timer = lv_timer_create(tick, FINDER_TICK_MS, NULL);
}

void demo_finder_exit(void)
{
    s_active = false;

    // 本应用是背光的唯一写入者，离场就得还回全亮，否则菜单会在息屏状态里出现。
    bsp_display_backlight((uint8_t)FINDER_IDLE_FULL_PERCENT);
    s_backlight = (uint8_t)FINDER_IDLE_FULL_PERCENT;

    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }

    if (s_scan_queue) {
        vQueueDelete(s_scan_queue);
        s_scan_queue = NULL;
    }
    if (s_input_queue) {
        vQueueDelete(s_input_queue);
        s_input_queue = NULL;
    }

    finder_view_delete();
    s_shown_valid = false;
}

esp_err_t demo_finder_start(void)
{
    s_scan_queue = xQueueCreate(FINDER_SCAN_Q_DEPTH, sizeof(finder_scan_result_t));
    s_input_queue = xQueueCreate(FINDER_INPUT_Q_DEPTH, sizeof(finder_input_t));
    if (!s_scan_queue || !s_input_queue) {
        return ESP_ERR_NO_MEM;
    }

    // 音频是软依赖：起不来就退化为纯视觉模式，不阻断寻找。
    esp_err_t audio_err = finder_audio_start();
    if (audio_err != ESP_OK) {
        ESP_LOGW(TAG, "蜂鸣不可用（%s）：退化为纯视觉提示", esp_err_to_name(audio_err));
    }

    s_scan_err = finder_scan_start(on_scan_result, NULL);
    if (s_scan_err != ESP_OK) {
        ESP_LOGE(TAG, "扫描启动失败: %s", esp_err_to_name(s_scan_err));
        return s_scan_err; // 扫描是本应用的核心，失败要如实上报
    }
    return ESP_OK;
}

esp_err_t demo_finder_stop(void)
{
    // 顺序：先停音频（不再向 I2S 写），再停扫描，最后落盘一次 NVS 标记。
    esp_err_t audio_err = finder_audio_stop();
    esp_err_t scan_err = finder_scan_stop();

    if (s_intro_dismiss_pending) {
        intro_store();
        s_intro_dismiss_pending = false;
    }

    if (audio_err != ESP_OK) {
        return audio_err;
    }
    return scan_err;
}
