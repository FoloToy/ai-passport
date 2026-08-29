#include "jpeg_view.h"

#include "bsp_display.h"
#include "jpeg_probe.h"
#include "jpeg_store.h"

#include "esp_jpeg_common.h"
#include "esp_jpeg_dec.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"

#include <stdint.h>
#include <string.h>

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320

static const char *TAG = "jpeg_view";
static TaskHandle_t s_worker;
static volatile bool s_busy;
static volatile bool s_active;
static lv_obj_t *s_screen;
static lv_obj_t *s_previous;
static lv_image_dsc_t s_descriptor;
static jpeg_view_result_cb_t s_callback;
static void *s_callback_user;
static uint8_t s_block[SCREEN_WIDTH * 16 * 2] __attribute__((aligned(16)));

static bool decode_to_frame(int *width, int *height)
{
    const uint8_t *jpeg = NULL;
    int jpeg_length = 0;
    if (jpeg_store_mmap(&jpeg, &jpeg_length) != 0) return false;

    jpeg_probe_t probe = jpeg_probe(jpeg, jpeg_length);
    if (probe != JPEG_PROBE_OK) {
        ESP_LOGW(TAG, "JPEG preflight failed: %s", jpeg_probe_str(probe));
        jpeg_store_unmap();
        return false;
    }

    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type = JPEG_PIXEL_FORMAT_RGB565_LE;
    config.block_enable = true;
    jpeg_dec_handle_t decoder = NULL;
    if (jpeg_dec_open(&config, &decoder) != JPEG_ERR_OK) {
        jpeg_store_unmap();
        return false;
    }

    jpeg_dec_io_t io;
    memset(&io, 0, sizeof(io));
    io.inbuf = (uint8_t *)jpeg;
    io.inbuf_len = jpeg_length;
    jpeg_dec_header_info_t info;
    bool ok = false;

    if (jpeg_dec_parse_header(decoder, &io, &info) == JPEG_ERR_OK &&
        info.width > 0 && info.height > 0 &&
        info.width <= SCREEN_WIDTH && info.height <= SCREEN_HEIGHT) {
        int output_length = 0;
        int process_count = 0;
        jpeg_dec_get_outbuf_len(decoder, &output_length);
        jpeg_dec_get_process_count(decoder, &process_count);
        uint32_t frame_bytes = (uint32_t)info.width * info.height * 2;
        if (output_length > 0 && output_length <= (int)sizeof(s_block) &&
            jpeg_frame_begin(frame_bytes) == 0) {
            io.outbuf = s_block;
            uint32_t offset = 0;
            ok = true;
            for (int i = 0; i < process_count; ++i) {
                jpeg_error_t result = jpeg_dec_process(decoder, &io);
                if (io.out_size > 0 &&
                    jpeg_frame_write(offset, s_block, io.out_size) == 0) {
                    offset += io.out_size;
                } else if (io.out_size > 0) {
                    ok = false;
                    break;
                }
                if (result != JPEG_ERR_OK) {
                    ok = false;
                    break;
                }
            }
            ok = ok && offset >= frame_bytes;
            if (ok) {
                *width = info.width;
                *height = info.height;
            }
        }
    }

    jpeg_dec_close(decoder);
    jpeg_store_unmap();
    return ok;
}

static bool show_frame(int width, int height)
{
    const uint8_t *frame = NULL;
    uint32_t frame_bytes = (uint32_t)width * height * 2;
    if (jpeg_frame_mmap(&frame, frame_bytes) != 0) return false;

    memset(&s_descriptor, 0, sizeof(s_descriptor));
    s_descriptor.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_descriptor.header.cf = LV_COLOR_FORMAT_RGB565;
    s_descriptor.header.w = width;
    s_descriptor.header.h = height;
    s_descriptor.header.stride = width * 2;
    s_descriptor.data = frame;
    s_descriptor.data_size = frame_bytes;

    if (!bsp_lvgl_lock(1000)) {
        jpeg_frame_unmap();
        return false;
    }
    s_previous = lv_screen_active();
    s_screen = lv_obj_create(NULL);
    lv_obj_remove_style_all(s_screen);
    lv_obj_set_style_bg_color(s_screen, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(s_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_t *image = lv_image_create(s_screen);
    lv_image_set_src(image, &s_descriptor);
    lv_obj_center(image);
    lv_screen_load(s_screen);
    bsp_lvgl_unlock();
    return true;
}

static void worker_task(void *argument)
{
    (void)argument;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        int width = 0;
        int height = 0;
        bool success = decode_to_frame(&width, &height) &&
                       show_frame(width, height);
        s_active = success;
        s_busy = false;
        if (s_callback) s_callback(success, s_callback_user);
    }
}

bool jpeg_view_init(jpeg_view_result_cb_t callback, void *user)
{
    s_callback = callback;
    s_callback_user = user;
    if (s_worker) return true;
    return xTaskCreate(worker_task, "safety_jpeg", 4096, NULL, 4,
                       &s_worker) == pdPASS;
}

bool jpeg_view_request(void)
{
    if (!s_worker || s_busy || s_active || !jpeg_store_has_valid()) return false;
    s_busy = true;
    xTaskNotifyGive(s_worker);
    return true;
}

bool jpeg_view_is_active(void) { return s_active; }
bool jpeg_view_is_busy(void) { return s_busy; }

void jpeg_view_exit(void)
{
    if (!s_active || !bsp_lvgl_lock(1000)) return;
    if (s_previous) lv_screen_load(s_previous);
    if (s_screen) lv_obj_delete(s_screen);
    s_screen = NULL;
    s_previous = NULL;
    s_active = false;
    jpeg_frame_unmap();
    bsp_lvgl_unlock();
}
