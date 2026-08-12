// main/demo_rps.c —— 石头剪刀布小游戏。
// 三张 RGB565 全屏素材直接从 Flash 读取，不分配全屏帧缓冲。
#include "demo.h"
#include "lvgl.h"
#include <stdint.h>

#define PARTICLE_COUNT 14
#define RING_COUNT 2
#define FRAME_WIDTH 240
#define FRAME_HEIGHT 320
#define FRAME_STRIDE (FRAME_WIDTH * 2)
#define FRAME_BYTES (FRAME_STRIDE * FRAME_HEIGHT)

extern const uint8_t s_rock_data[] asm("_binary_rock_rgb565_start");
extern const uint8_t s_scissors_data[] asm("_binary_scissors_rgb565_start");
extern const uint8_t s_paper_data[] asm("_binary_paper_rgb565_start");

typedef enum {
    RPS_ROCK,
    RPS_SCISSORS,
    RPS_PAPER,
    RPS_COUNT,
} rps_move_t;

static const char *MOVE_NAMES[RPS_COUNT] = { "ROCK", "SCISSORS", "PAPER" };
static const uint32_t MOVE_COLORS[RPS_COUNT] = { 0xFF4A32, 0x39BFFF, 0xFFD84A };
static const uint32_t PARTICLE_COLORS[] = {
    0xFF4A32, 0xFF8A38, 0x39BFFF, 0x8DEBFF, 0xFFD84A, 0xFFF3A1,
};
static const int8_t PARTICLE_VX[PARTICLE_COUNT] = {
    -11, -8, -4, 1, 6, 10, 12, 9, 5, 0, -5, -9, -12, 4,
};
static const int8_t PARTICLE_VY[PARTICLE_COUNT] = {
    -3, -8, -11, -12, -10, -6, 0, 7, 11, 12, 10, 6, 1, 4,
};

static const lv_image_dsc_t ROCK_IMAGE = {
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.flags = 0,
    .header.w = FRAME_WIDTH,
    .header.h = FRAME_HEIGHT,
    .header.stride = FRAME_STRIDE,
    .data_size = FRAME_BYTES,
    .data = s_rock_data,
};

static const lv_image_dsc_t SCISSORS_IMAGE = {
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.flags = 0,
    .header.w = FRAME_WIDTH,
    .header.h = FRAME_HEIGHT,
    .header.stride = FRAME_STRIDE,
    .data_size = FRAME_BYTES,
    .data = s_scissors_data,
};

static const lv_image_dsc_t PAPER_IMAGE = {
    .header.magic = LV_IMAGE_HEADER_MAGIC,
    .header.cf = LV_COLOR_FORMAT_RGB565,
    .header.flags = 0,
    .header.w = FRAME_WIDTH,
    .header.h = FRAME_HEIGHT,
    .header.stride = FRAME_STRIDE,
    .data_size = FRAME_BYTES,
    .data = s_paper_data,
};

static const lv_image_dsc_t *MOVE_IMAGES[RPS_COUNT] = {
    &ROCK_IMAGE, &SCISSORS_IMAGE, &PAPER_IMAGE,
};

static lv_obj_t *s_scr;
static lv_obj_t *s_image;
static lv_obj_t *s_title;
static lv_obj_t *s_status;
static lv_obj_t *s_particles[PARTICLE_COUNT];
static lv_obj_t *s_rings[RING_COUNT];
static lv_timer_t *s_timer;
static uint32_t s_tick;
static uint8_t s_burst_tick;
static bool s_attract;

static lv_obj_t *make_label(lv_obj_t *parent, const char *text, int y,
                            const lv_font_t *font, uint32_t color)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_width(label, 224);
    lv_obj_set_pos(label, 8, y);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(label, lv_color_hex(0x05060B), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_70, 0);
    lv_obj_set_style_pad_ver(label, 5, 0);
    lv_obj_set_style_radius(label, 0, 0);
    return label;
}

static void set_fx_hidden(bool hidden)
{
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        if (hidden) lv_obj_add_flag(s_particles[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(s_particles[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < RING_COUNT; i++) {
        if (hidden) lv_obj_add_flag(s_rings[i], LV_OBJ_FLAG_HIDDEN);
        else lv_obj_remove_flag(s_rings[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void animate_fx(uint32_t time)
{
    for (int i = 0; i < PARTICLE_COUNT; i++) {
        uint32_t phase = (time + (uint32_t)i * 5U) % 48U;
        int size = 3 + (int)(phase / 16U);
        int x = 120 + PARTICLE_VX[i] * (int)phase / 4 - size / 2;
        int y = 158 + PARTICLE_VY[i] * (int)phase / 4 - size / 2;
        lv_obj_set_pos(s_particles[i], x, y);
        lv_obj_set_size(s_particles[i], size, size);
        lv_obj_set_style_opa(s_particles[i], (lv_opa_t)(255U - phase * 4U), 0);
    }

    for (int i = 0; i < RING_COUNT; i++) {
        uint32_t phase = (time + (uint32_t)i * 18U) % 42U;
        int size = 18 + (int)phase * 4;
        lv_obj_set_pos(s_rings[i], 120 - size / 2, 158 - size / 2);
        lv_obj_set_size(s_rings[i], size, size);
        lv_obj_set_style_opa(s_rings[i], (lv_opa_t)(210U - phase * 4U), 0);
    }
}

static void tick(lv_timer_t *timer)
{
    (void)timer;
    s_tick++;
    if (s_attract) {
        animate_fx(s_tick);
        return;
    }

    if (s_burst_tick < 28) {
        animate_fx((uint32_t)s_burst_tick * 2U);
        s_burst_tick++;
    } else {
        set_fx_hidden(true);
    }
}

static void choose_move(rps_move_t move)
{
    s_attract = false;
    s_burst_tick = 0;
    lv_image_set_src(s_image, MOVE_IMAGES[move]);
    lv_obj_remove_flag(s_image, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_title, MOVE_NAMES[move]);
    lv_obj_set_style_text_color(s_title, lv_color_hex(MOVE_COLORS[move]), 0);
    lv_label_set_text(s_status, "PRESS ANOTHER KEY TO CHANGE");
    set_fx_hidden(false);
    animate_fx(0);
}

void demo_rps_enter(void)
{
    s_tick = 0;
    s_burst_tick = 0;
    s_attract = true;

    s_scr = lv_obj_create(NULL);
    lv_obj_remove_flag(s_scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(s_scr, lv_color_hex(0x05060B), 0);
    lv_obj_set_style_border_width(s_scr, 0, 0);
    lv_obj_set_style_pad_all(s_scr, 0, 0);

    s_image = lv_image_create(s_scr);
    lv_obj_set_pos(s_image, 0, 0);
    lv_obj_add_flag(s_image, LV_OBJ_FLAG_HIDDEN);

    for (int i = 0; i < RING_COUNT; i++) {
        s_rings[i] = lv_obj_create(s_scr);
        lv_obj_remove_flag(s_rings[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_bg_opa(s_rings[i], LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(s_rings[i], 3, 0);
        lv_obj_set_style_border_color(s_rings[i], lv_color_hex(i ? 0x39BFFF : 0xFF4A32), 0);
        lv_obj_set_style_radius(s_rings[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_pad_all(s_rings[i], 0, 0);
    }

    for (int i = 0; i < PARTICLE_COUNT; i++) {
        s_particles[i] = lv_obj_create(s_scr);
        lv_obj_remove_flag(s_particles[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_style_border_width(s_particles[i], 0, 0);
        lv_obj_set_style_pad_all(s_particles[i], 0, 0);
        lv_obj_set_style_radius(s_particles[i], 0, 0);
        lv_obj_set_style_bg_color(s_particles[i],
            lv_color_hex(PARTICLE_COLORS[i % 6]), 0);
    }

    s_title = make_label(s_scr, "RPS ARENA", 18,
                         &lv_font_montserrat_20, 0xFFFFFF);
    s_status = make_label(s_scr, "CHOOSE YOUR MOVE", 61,
                          &lv_font_montserrat_14, 0xDCEEFF);
    make_label(s_scr, "UP ROCK   DOWN SCISSORS\nOK PAPER", 271,
               &lv_font_montserrat_14, 0xFFFFFF);

    animate_fx(0);
    s_timer = lv_timer_create(tick, 40, NULL);
    lv_screen_load(s_scr);
}

void demo_rps_exit(void)
{
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = NULL;
    }
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
        s_image = s_title = s_status = NULL;
        for (int i = 0; i < PARTICLE_COUNT; i++) s_particles[i] = NULL;
        for (int i = 0; i < RING_COUNT; i++) s_rings[i] = NULL;
    }
}

void demo_rps_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (ev != BSP_BTN_PRESS) return;
    if (btn == BSP_BTN_UP) choose_move(RPS_ROCK);
    else if (btn == BSP_BTN_DOWN) choose_move(RPS_SCISSORS);
    else if (btn == BSP_BTN_OK) choose_move(RPS_PAPER);
}
