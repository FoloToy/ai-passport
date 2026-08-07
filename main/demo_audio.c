// main/demo_audio.c —— 播 1kHz 方波 / 录 3 秒后回放。
// 音频收发会阻塞较久,故放到独立任务里跑,不占用按键回调与 LVGL 任务。
#include "demo.h"
#include "bsp_audio.h"
#include "bsp_display.h"   // bsp_lvgl_lock / bsp_lvgl_unlock(音频任务里操作 LVGL 要加锁)
#include "ui_pixel.h"
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdlib.h>
#include <string.h>

static const char *TAG = "demo_audio";

#define SAMPLE_RATE   16000
#define TONE_HZ        1000
#define TONE_MS        1000
#define RECORD_SEC        3
#define CHUNK_SAMPLES   512          // 每次收发的采样数,控制临时缓冲大小

static lv_obj_t *s_scr, *s_status, *s_mascot;
static TaskHandle_t s_task;
static volatile int s_req;           // 1=播音 2=录音回放 0=空闲

// LVGL 对象只能在持锁时操作;本函数从音频任务调用,故内部加锁。
static void set_status(const char *text) {
    if (!bsp_lvgl_lock(500)) return;
    if (s_status) lv_label_set_text(s_status, text);
    bsp_lvgl_unlock();
}

static void play_tone(void) {
    set_status("playing 1kHz...");
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) { set_status("format failed"); return; }
    bsp_audio_set_volume(80);

    int16_t *buf = malloc(CHUNK_SAMPLES * sizeof(int16_t));
    if (!buf) { set_status("out of memory"); return; }

    const int period = SAMPLE_RATE / TONE_HZ;        // 每个方波周期的采样数
    int total = SAMPLE_RATE * TONE_MS / 1000;
    int phase = 0;
    while (total > 0) {
        int n = total < CHUNK_SAMPLES ? total : CHUNK_SAMPLES;
        for (int i = 0; i < n; i++) {
            buf[i] = (phase < period / 2) ? 6000 : -6000;
            if (++phase >= period) phase = 0;
        }
        bsp_audio_write(buf, (size_t)n * sizeof(int16_t));
        total -= n;
    }
    free(buf);
    set_status("done. OK: tone  UP: record");
}

static void record_and_play(void) {
    if (bsp_audio_set_format(SAMPLE_RATE, 16, 1) != ESP_OK) { set_status("format failed"); return; }

    size_t total = (size_t)SAMPLE_RATE * RECORD_SEC;
    int16_t *rec = malloc(total * sizeof(int16_t));   // 3s @16k 16bit = 96KB
    if (!rec) {
        // C3 无 PSRAM,96KB 可能分配不到 —— 明确告知而不是静默失败
        ESP_LOGE(TAG, "录音缓冲 %u 字节分配失败(C3 内存紧张,可缩短 RECORD_SEC)",
                 (unsigned)(total * sizeof(int16_t)));
        set_status("record buffer alloc failed");
        return;
    }

    set_status("recording 3s... speak now");
    size_t got = 0;
    while (got < total) {
        size_t n = (total - got) < CHUNK_SAMPLES ? (total - got) : CHUNK_SAMPLES;
        if (bsp_audio_read(rec + got, n * sizeof(int16_t)) != ESP_OK) break;
        got += n;
    }

    set_status("playing back...");
    bsp_audio_set_volume(80);
    for (size_t off = 0; off < got; off += CHUNK_SAMPLES) {
        size_t n = (got - off) < CHUNK_SAMPLES ? (got - off) : CHUNK_SAMPLES;
        bsp_audio_write(rec + off, n * sizeof(int16_t));
    }
    free(rec);
    set_status("done. OK: tone  UP: record");
}

static void audio_task(void *arg) {
    (void)arg;
    for (;;) {
        if (s_req == 1)      { s_req = 0; play_tone(); }
        else if (s_req == 2) { s_req = 0; record_and_play(); }
        else vTaskDelay(pdMS_TO_TICKS(50));
    }
}

void demo_audio_enter(void) {
    s_scr = ui_pixel_screen_create("AUDIO");
    lv_obj_t *panel = ui_pixel_panel_create(s_scr, 18, 62, 204, 168, UI_PAPER);

    lv_obj_t *record = ui_pixel_panel_create(panel, 58, 12, 72, 72, UI_INK);
    lv_obj_t *disc = lv_obj_create(record);
    lv_obj_set_size(disc, 36, 36);
    lv_obj_set_style_radius(disc, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(disc, lv_color_hex(UI_RED), 0);
    lv_obj_set_style_border_width(disc, 0, 0);
    lv_obj_center(disc);

    s_status = lv_label_create(panel);
    lv_obj_set_style_text_color(s_status, lv_color_hex(UI_INK), 0);
    lv_obj_set_style_text_align(s_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_status, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(s_status, 176);
    lv_label_set_text(s_status, "OK: 1kHz TONE\nUP: RECORD + PLAY");
    lv_obj_align(s_status, LV_ALIGN_BOTTOM_MID, 0, -9);

    s_mascot = ui_pixel_mascot_create(s_scr, 101, 238);

    s_req = 0;
    if (!s_task) xTaskCreate(audio_task, "demo_audio", 4096, NULL, 4, &s_task);
    lv_screen_load(s_scr);
}

void demo_audio_exit(void) {
    s_req = 0;
    if (s_task) { vTaskDelete(s_task); s_task = NULL; }
    if (s_scr) { lv_obj_delete(s_scr); s_scr = NULL; s_status = s_mascot = NULL; }
}

void demo_audio_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (ev != BSP_BTN_CLICK) return;
    if (btn == BSP_BTN_OK) s_req = 1;
    if (btn == BSP_BTN_UP) s_req = 2;
    if (btn == BSP_BTN_OK || btn == BSP_BTN_UP) ui_pixel_mascot_jump(s_mascot);
}
