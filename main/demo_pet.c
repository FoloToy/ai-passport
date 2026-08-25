// main/demo_pet.c —— 数据驱动的宠物 demo
//
// 行为 (动作/帧/速度/跳跃) 全部来自 main/pet/pet_manifest.h,
// 由 tools/prep_pet.py 从 pets/ 的 PNG 自动汇总生成, 本文件不含任何动作硬编码。
// motion 字段 (idle/moveforward/sprintforward/moveup/sprintup) 决定移动方式, 与动物种类无关。
// 资源: tools/png2lvgl.py + tools/prep_pet.py 生成 (56x56, 格式随 prep_pet.py, 朝右归一)
#include "demo.h"
#include "bsp_display.h"
#include "ui_pixel.h"
#include "pet_manifest.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

/* ===== 显示 ===== */
#define PET_SCALE      460                                   /* 256=100% → 460 ≈ 1.8x */
#define PET_DRAW_W     ((PET_SPRITE_W * PET_SCALE) / 256)    /* 56*1.8 ≈ 101 */
#define PET_DRAW_H     ((PET_SPRITE_H * PET_SCALE) / 256)
#define GROUND_Y       286                                   /* 草地顶面 */
#define PET_Y_BASE     (GROUND_Y - PET_DRAW_H)
#define WALK_MIN       2
#define WALK_MAX       (240 - PET_DRAW_W - 2)

#define TICK_MS        40                                    /* 主循环节拍 */

/* ===== 运行时状态 ===== */
static lv_obj_t   *s_scr;
static lv_obj_t   *s_cat;
static lv_timer_t *s_tick;

static uint8_t s_act;          /* 当前动作下标 (pet_actions[]) */
static uint8_t s_frame;        /* 当前帧下标 */
static uint16_t s_frame_acc;   /* 换帧累加器 (ms) */
static uint16_t s_hold_acc;    /* 静止动作持续累加器 (ms) */
static int16_t  s_x;           /* 猫的视觉 x */
static int8_t   s_dir;         /* +1 右 / -1 左 */
static bool     s_face_right;

/* 朝左时按行水平镜像到静态 RAM 缓冲 (零 malloc, 适配无 PSRAM 设备)。
 * 缓冲按 ARGB8888(4B/px) 上限预留, 格式随宠物自动适配。 */
static uint8_t       s_flip_data[PET_SPRITE_W * PET_SPRITE_H * 4] __attribute__((aligned(4)));
static lv_draw_buf_t s_flip_db;

/* 缓存上一次实际绘制的 (动作, 帧, 朝向) 避免每拍重复 set_src / 翻转 */
static uint8_t s_drawn_act   = 0xFF;
static uint8_t s_drawn_frame = 0xFF;
static bool    s_drawn_face  = true;

static const lv_image_dsc_t *pet_resolve(const lv_image_dsc_t *frm, bool right)
{
    if (right) return frm;
    uint32_t w   = frm->header.w;
    uint32_t h   = frm->header.h;
    uint32_t cf  = frm->header.cf;
    const uint8_t *s = frm->data;
    uint8_t *d = s_flip_db.data;
    if (cf == LV_COLOR_FORMAT_RGB565A8) {
        /* 两独立块: 颜色(RGB565, 2B/px) + alpha(8bit, 1B/px), 各自行内水平镜像 */
        uint32_t cstride = w * 2;
        uint32_t aoff    = cstride * h;
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                const uint8_t *sc = s + y * cstride + x * 2;
                uint8_t *dc = d + y * cstride + (w - 1 - x) * 2;
                dc[0] = sc[0]; dc[1] = sc[1];
                const uint8_t *sa = s + aoff + y * w + x;
                uint8_t *da = d + aoff + y * w + (w - 1 - x);
                *da = *sa;
            }
        }
    } else {
        /* ARGB8888 / RGB565 等单块打包格式, 按 bpp 整像素镜像 */
        uint32_t bpp = (cf == LV_COLOR_FORMAT_RGB565) ? 2u : 4u;
        for (uint32_t y = 0; y < h; y++) {
            for (uint32_t x = 0; x < w; x++) {
                const uint8_t *sp = s + (y * w + x) * bpp;
                uint8_t *dp = d + (y * w + (w - 1 - x)) * bpp;
                for (uint32_t c = 0; c < bpp; c++) dp[c] = sp[c];
            }
        }
    }
    return (const lv_image_dsc_t *)&s_flip_db;
}

static void pet_draw(void)
{
    if (s_act == s_drawn_act && s_frame == s_drawn_frame &&
        s_face_right == s_drawn_face) {
        return;
    }
    lv_image_set_src(s_cat, pet_resolve(pet_actions[s_act].frames[s_frame], s_face_right));
    s_drawn_act   = s_act;
    s_drawn_frame = s_frame;
    s_drawn_face  = s_face_right;
}

/* 取一个随机静止动作 (休息用); 没有则返回 0xFF */
static uint8_t pick_rest(void)
{
    if (PET_REST_N == 0) return 0xFF;
    return pet_rest_pool[lv_rand(0, PET_REST_N - 1)];
}

/* 取一个随机移动动作, 尽量不同于 cur (移动/切换用); 没有则返回 0xFF */
static uint8_t pick_move(uint8_t cur)
{
    if (PET_MOVE_N == 0) return 0xFF;
    if (PET_MOVE_N == 1) return pet_move_pool[0];
    uint8_t i;
    do { i = pet_move_pool[lv_rand(0, PET_MOVE_N - 1)]; } while (i == cur);
    return i;
}

static void enter_act(uint8_t act)
{
    s_act       = act;
    s_frame     = 0;
    s_frame_acc = 0;
    s_hold_acc  = 0;
    lv_obj_set_y(s_cat, PET_Y_BASE);   /* 切动作时归位, 避免 hop 残影 */
    pet_draw();
}

/* 起点: 优先移动动作, 否则第 0 个 */
static uint8_t start_act(void)
{
    return (PET_MOVE_N > 0) ? pet_move_pool[0] : 0;
}

/* ===== 主节拍 ===== */
static void tick_cb(lv_timer_t *t)
{
    (void)t;
    const pet_action_t *def = &pet_actions[s_act];

    /* 1) 换帧 */
    s_frame_acc += TICK_MS;
    if (s_frame_acc >= def->frame_ms) {
        s_frame_acc = 0;
        s_frame = (uint8_t)((s_frame + 1) % def->n_frames);
        pet_draw();
    }

    /* 2) 移动 / 静止 */
    if (!def->stationary) {
        s_x += (int16_t)(s_dir * (int8_t)def->step_px);
        if (s_x <= WALK_MIN)      { s_x = WALK_MIN;  uint8_t r = pick_rest(); if (r != 0xFF) enter_act(r); else { s_dir = 1;  pet_draw(); } }
        else if (s_x >= WALK_MAX) { s_x = WALK_MAX; uint8_t r = pick_rest(); if (r != 0xFF) enter_act(r); else { s_dir = -1; pet_draw(); } }
        lv_obj_set_x(s_cat, s_x);

        /* 移动中随机切换成另一个移动动作 (让 jump/run 自然出现) */
        if (PET_MOVE_N > 1 && lv_rand(0, 299) == 0) {
            uint8_t m = pick_move(s_act);
            if (m != 0xFF) enter_act(m);
        }
    } else {
        s_hold_acc += TICK_MS;
        if (s_hold_acc >= def->hold_ms) {
            /* 休息结束 → 转身继续移动 (无移动动作则保持原地) */
            s_dir = (int8_t)-s_dir;
            s_face_right = (s_dir > 0);
            pet_draw();
            uint8_t m = pick_move(s_act);
            enter_act((m != 0xFF) ? m : s_act);
            return;
        }
    }

    /* 3) 垂直跳跃 (通用抛物线: 由 hop_max 与帧数推导, 任意动作适用) */
    if (def->hop_max > 0) {
        uint8_t n = def->n_frames;
        int32_t dy;
        if (n > 1) {
            int32_t f   = s_frame;
            int32_t num = f * (n - 1 - f);          /* 两端为 0, 中段最大 */
            int32_t den = (int32_t)(n - 1) * (n - 1);
            dy = -((int32_t)def->hop_max * 4 * num) / den;
        } else {
            dy = 0;
        }
        lv_obj_set_y(s_cat, PET_Y_BASE + (int16_t)dy);
    }
}

/* ===== 生命周期 ===== */
void demo_pet_enter(void)
{
    s_scr = ui_pixel_screen_create("PET");

#if PET_HAS_BG
    /* 全屏背景图: 置于标题/草地之下 (240x320) */
    lv_obj_t *bg = lv_image_create(s_scr);
    lv_obj_set_pos(bg, 0, 0);
    lv_obj_set_size(bg, 240, 320);
    lv_image_set_src(bg, &pet_bg);
    lv_obj_move_to_index(bg, 0);
#endif

    s_cat = lv_image_create(s_scr);
    lv_obj_set_size(s_cat, PET_DRAW_W, PET_DRAW_H);
    lv_image_set_inner_align(s_cat, LV_IMAGE_ALIGN_CENTER);
    lv_image_set_scale(s_cat, PET_SCALE);

    /* 静态 RAM 镜像缓冲 (零 malloc); 格式/stride 跟随宠物, 朝左时按行翻转 */
    lv_color_format_t flip_cf = pet_actions[0].frames[0]->header.cf;
    uint32_t flip_stride = lv_draw_buf_width_to_stride(PET_SPRITE_W, flip_cf);
    lv_draw_buf_init(&s_flip_db, PET_SPRITE_W, PET_SPRITE_H,
                     flip_cf, flip_stride,
                     s_flip_data, sizeof(s_flip_data));

    s_x          = WALK_MIN;
    s_dir        = 1;
    s_face_right = true;
    s_drawn_act   = 0xFF;
    s_drawn_frame = 0xFF;
    s_drawn_face  = true;
    lv_obj_set_pos(s_cat, s_x, PET_Y_BASE);
    pet_draw();

    enter_act(start_act());

    s_tick = lv_timer_create(tick_cb, TICK_MS, NULL);
    lv_screen_load(s_scr);
}

void demo_pet_exit(void)
{
    if (s_tick) { lv_timer_delete(s_tick); s_tick = NULL; }
    if (s_scr)  { lv_obj_delete(s_scr);    s_scr  = NULL; }
    /* s_flip_db / s_flip_data 是静态数组, 无需释放 */
    s_cat   = NULL;
}

void demo_pet_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    /* 短按任意键: 切换到下一个动作, 方便逐个查看美术资源 */
    if (ev != BSP_BTN_CLICK) return;
    (void)btn;
    uint8_t next = (uint8_t)((s_act + 1) % PET_ACT_COUNT);
    enter_act(next);
}
