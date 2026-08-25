// main/demo_black.c —— 4 动作小猫 (walk/jump/sleep/groom)
// 行为由动作名 (cat_*.h 中的 *_1/_2/...) 统一驱动:
//   walk  → 慢速水平移动 (WALK_STEP=1 px/tick)
//   jump  → 快速水平 + 垂直跳跃 (JUMP_STEP=2, 4 帧 hop 抛物线)
//   sleep → 原地循环 (5 帧, 长停顿)
//   groom → 原地循环 (3 帧, 短停顿)
//
// 资源: tools/png2lvgl.py 从 cats/<action_frame_dir>.png 一站式生成
//       16 帧 56x56 ARGB8888, 已归一为朝右 (见 cat_sprites.h)
//
// 易踩的坑 (本文件已规避):
//   1) lv_image_set_scale 单位是 256=100% (CAT_SCALE=460 ≈ 1.8x)
//   2) 精灵必须等比正方形缩放 (否则非等比会变形 / 切头)
//   3) 朝左镜像用 lv_draw_buf_init 绑定【静态数组】(不 malloc),
//      避免无 PSRAM 设备上 lv_draw_buf_create 失败导致空指针 panic 重启
#include "demo.h"
#include "bsp_display.h"
#include "ui_pixel.h"
#include "cat_sprites.h"
#include "lvgl.h"
#include <stdbool.h>
#include <stdint.h>

/* ===== 显示 ===== */
#define CAT_SCALE      460                                   /* 256=100% → 460 ≈ 1.8x */
#define CAT_DRAW_W     ((CAT_SPRITE_W * CAT_SCALE) / 256)    /* 56*1.8 ≈ 101 */
#define CAT_DRAW_H     ((CAT_SPRITE_H * CAT_SCALE) / 256)
#define GROUND_Y       286                                   /* 草地顶面 */
#define CAT_Y_BASE     (GROUND_Y - CAT_DRAW_H)
#define WALK_MIN       2
#define WALK_MAX       (240 - CAT_DRAW_W - 2)

#define TICK_MS        40                                    /* 主循环节拍 */
#define HOP_MAX_PX     12                                    /* jump 垂直跳跃幅度 */

/* ===== 动作定义 ===== */
typedef enum {
    ACT_WALK  = 0,
    ACT_JUMP  = 1,
    ACT_SLEEP = 2,
    ACT_GROOM = 3,
    ACT_COUNT,
} cat_act_t;

static const lv_image_dsc_t *const f_walk[]  = {
    &cat_walk_1, &cat_walk_2, &cat_walk_3, &cat_walk_4,
};
static const lv_image_dsc_t *const f_jump[]  = {
    &cat_jump_1, &cat_jump_2, &cat_jump_3, &cat_jump_4,
};
static const lv_image_dsc_t *const f_sleep[] = {
    &cat_sleep_1, &cat_sleep_2, &cat_sleep_3, &cat_sleep_4, &cat_sleep_5,
};
static const lv_image_dsc_t *const f_groom[] = {
    &cat_groom_1, &cat_groom_2, &cat_groom_3,
};

typedef struct {
    const lv_image_dsc_t *const *frames;
    uint8_t  n_frames;
    uint16_t frame_ms;     /* 换帧周期 (ms) */
    uint16_t hold_ms;      /* 持续时长; 0 = 直到撞墙 */
    const char *label;
} cat_act_def_t;

static const cat_act_def_t s_acts[ACT_COUNT] = {
    /* ACT_WALK  */ { f_walk,  4, 130,    0, "WALK"  },
    /* ACT_JUMP  */ { f_jump,  4, 110,    0, "JUMP"  },
    /* ACT_SLEEP */ { f_sleep, 5, 450, 5000, "SLEEP" },
    /* ACT_GROOM */ { f_groom, 3, 300, 2500, "GROOM" },
};

/* 撞墙休息时随机选 sleep 或 groom */
static const cat_act_t s_rest_pool[] = { ACT_SLEEP, ACT_GROOM };
#define REST_POOL_N  (sizeof(s_rest_pool) / sizeof(s_rest_pool[0]))

/* ===== 行为分类: 根据动作名返回移动特征 =====
 *   walk  → 慢速水平
 *   jump  → 快速水平 + 垂直 hop
 *   sleep → 原地不动
 *   groom → 原地不动
 */
typedef struct {
    uint8_t step_px;     /* 每 tick 水平位移像素 */
    uint8_t hop_max;     /* jump 垂直跳跃幅度 (像素) */
    bool    stationary;  /* true = 原地不动 */
} act_behavior_t;

static act_behavior_t classify(cat_act_t a)
{
    switch (a) {
        case ACT_WALK:  return (act_behavior_t){ .step_px=1, .hop_max=0,  .stationary=false };
        case ACT_JUMP:  return (act_behavior_t){ .step_px=2, .hop_max=HOP_MAX_PX, .stationary=false };
        case ACT_SLEEP: return (act_behavior_t){ .step_px=0, .hop_max=0,  .stationary=true  };
        case ACT_GROOM: return (act_behavior_t){ .step_px=0, .hop_max=0,  .stationary=true  };
        default:        return (act_behavior_t){ .step_px=0, .hop_max=0,  .stationary=true  };
    }
}

/* jump 4 帧的垂直偏移 (px): 0, -6, -12, -6 — 抛物线顶点在中段 */
static const int8_t s_jump_hop[4] = { 0, -6, -12, -6 };

/* ===== 运行时状态 ===== */
static lv_obj_t   *s_scr;
static lv_obj_t   *s_cat;
static lv_obj_t   *s_label;
static lv_timer_t *s_tick;

static cat_act_t s_act;           /* 当前动作 */
static uint8_t   s_frame;         /* 当前帧下标 */
static uint16_t  s_frame_acc;     /* 换帧累加器 (ms) */
static uint16_t  s_hold_acc;      /* 静止动作持续累加器 (ms) */
static int16_t   s_x;             /* 猫的视觉 x */
static int8_t    s_dir;           /* +1 右 / -1 左 */
static bool      s_face_right;

/* ===== 朝左的水平镜像 (lv_draw_buf_t 静态缓冲, 零 malloc) =====
 * 180° 旋转会上下颠倒, 这里用按行左右镜像写 RAM, 复用同一份 Flash。
 * 关键: 必须 lv_draw_buf_init 绑定【预分配静态数组】, 不能 lv_draw_buf_create
 * (malloc 12KB, 无 PSRAM 设备上会 NULL → 空指针 panic 重启回菜单)。*/
static uint8_t       s_flip_data[CAT_SPRITE_W * CAT_SPRITE_H * 4] __attribute__((aligned(4)));
static lv_draw_buf_t s_flip_db;

/* 缓存上一次实际绘制的 (动作, 帧, 朝向) 避免每拍重复 set_src / 翻转 */
static cat_act_t s_drawn_act   = (cat_act_t)ACT_COUNT;
static uint8_t   s_drawn_frame = 0xFF;
static bool      s_drawn_face  = true;

static const lv_image_dsc_t *cat_resolve(const lv_image_dsc_t *frm, bool right)
{
    if (right) return frm;
    uint32_t w   = frm->header.w;
    uint32_t h   = frm->header.h;
    uint32_t bpp = (frm->header.cf == LV_COLOR_FORMAT_ARGB8888) ? 4u
                 : (frm->header.cf == LV_COLOR_FORMAT_RGB565)   ? 2u : 4u;
    const uint8_t *s = frm->data;
    uint8_t *d = s_flip_db.data;
    for (uint32_t y = 0; y < h; y++) {
        const uint8_t *srow = s + y * w * bpp;
        for (uint32_t x = 0; x < w; x++) {
            const uint8_t *sp = srow + x * bpp;
            uint8_t *dp = d + (y * w + (w - 1 - x)) * bpp;
            for (uint32_t c = 0; c < bpp; c++) dp[c] = sp[c];
        }
    }
    return (const lv_image_dsc_t *)&s_flip_db;
}

static void cat_draw(void)
{
    if (s_act == s_drawn_act && s_frame == s_drawn_frame &&
        s_face_right == s_drawn_face) {
        return;
    }
    lv_image_set_src(s_cat, cat_resolve(s_acts[s_act].frames[s_frame], s_face_right));
    s_drawn_act   = s_act;
    s_drawn_frame = s_frame;
    s_drawn_face  = s_face_right;
}

static void enter_act(cat_act_t act)
{
    s_act       = act;
    s_frame     = 0;
    s_frame_acc = 0;
    s_hold_acc  = 0;
    /* 切动作时立刻归位 (避免上一动作 hop 残影) */
    lv_obj_set_y(s_cat, CAT_Y_BASE);
    cat_draw();
    if (s_label) lv_label_set_text(s_label, s_acts[act].label);
}

static void pick_rest(void)
{
    /* 撞墙 → 随机睡或舔 */
    enter_act(s_rest_pool[lv_rand(0, REST_POOL_N - 1)]);
}

/* ===== 主节拍 ===== */
static void tick_cb(lv_timer_t *t)
{
    (void)t;
    const cat_act_def_t *def = &s_acts[s_act];
    act_behavior_t b = classify(s_act);

    /* 1) 换帧 */
    s_frame_acc += TICK_MS;
    if (s_frame_acc >= def->frame_ms) {
        s_frame_acc = 0;
        s_frame = (uint8_t)((s_frame + 1) % def->n_frames);
        cat_draw();
    }

    /* 2) 移动 / 静止计时 */
    if (!b.stationary) {
        s_x += (int16_t)(s_dir * (int8_t)b.step_px);
        if (s_x <= WALK_MIN)      { s_x = WALK_MIN;  pick_rest(); }
        else if (s_x >= WALK_MAX) { s_x = WALK_MAX; pick_rest(); }
        lv_obj_set_x(s_cat, s_x);
    } else {
        s_hold_acc += TICK_MS;
        if (s_hold_acc >= def->hold_ms) {
            /* 休息结束 → 转身继续走 */
            s_dir = (int8_t)-s_dir;
            s_face_right = (s_dir > 0);
            cat_draw();
            enter_act(ACT_WALK);
            return;
        }
    }

    /* 3) 垂直跳跃 (仅 jump) */
    if (b.hop_max > 0) {
        uint8_t fi = (s_frame < def->n_frames) ? s_frame : 0;
        int16_t dy = (def->n_frames == 4) ? s_jump_hop[fi] : 0;
        lv_obj_set_y(s_cat, CAT_Y_BASE + dy);
    }

    /* 4) WALK 中偶发切换到 JUMP, 让 jump 动作真的出现 (~每 5 秒一次) */
    if (s_act == ACT_WALK && lv_rand(0, 119) == 0) {
        enter_act(ACT_JUMP);
    }
}

/* ===== 生命周期 ===== */
void demo_black_enter(void)
{
    s_scr = ui_pixel_screen_create("BLACK");

    s_cat = lv_image_create(s_scr);
    lv_obj_set_size(s_cat, CAT_DRAW_W, CAT_DRAW_H);
    lv_image_set_inner_align(s_cat, LV_IMAGE_ALIGN_CENTER);
    lv_image_set_scale(s_cat, CAT_SCALE);

    /* 静态 RAM 镜像缓冲 (零 malloc) */
    lv_draw_buf_init(&s_flip_db, CAT_SPRITE_W, CAT_SPRITE_H,
                     LV_COLOR_FORMAT_ARGB8888, CAT_SPRITE_W * 4,
                     s_flip_data, sizeof(s_flip_data));

    s_x          = WALK_MIN;
    s_dir        = 1;
    s_face_right = true;
    /* 重置绘制缓存, 确保重新进入时一定重设 src */
    s_drawn_act   = (cat_act_t)ACT_COUNT;
    s_drawn_frame = 0xFF;
    s_drawn_face  = true;
    lv_obj_set_pos(s_cat, s_x, CAT_Y_BASE);
    cat_draw();

    /* 状态文字 */
    s_label = lv_label_create(s_scr);
    lv_obj_set_style_text_color(s_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(s_label, LV_ALIGN_TOP_MID, 0, 34);

    enter_act(ACT_WALK);

    s_tick = lv_timer_create(tick_cb, TICK_MS, NULL);
    lv_screen_load(s_scr);
}

void demo_black_exit(void)
{
    if (s_tick) { lv_timer_delete(s_tick); s_tick = NULL; }
    if (s_scr)  { lv_obj_delete(s_scr);    s_scr  = NULL; }
    /* s_flip_db / s_flip_data 是静态数组, 无需释放 */
    s_cat   = NULL;
    s_label = NULL;
}

void demo_black_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    /* 短按任意键: 切换到下一个动作, 方便逐个查看美术资源 */
    if (ev != BSP_BTN_CLICK) return;
    (void)btn;
    cat_act_t next = (cat_act_t)((s_act + 1) % ACT_COUNT);
    enter_act(next);
}