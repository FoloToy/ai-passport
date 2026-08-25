/* 由 prep_pet.py 自动生成 —— 勿手改 */
#ifndef PET_MANIFEST_H
#define PET_MANIFEST_H

#include "lvgl.h"

#define PET_HAS_BG  0

#include "pet_groom_1.h"
#include "pet_groom_2.h"
#include "pet_groom_3.h"
#include "pet_jump_1.h"
#include "pet_jump_2.h"
#include "pet_jump_3.h"
#include "pet_jump_4.h"
#include "pet_sleep_1.h"
#include "pet_sleep_2.h"
#include "pet_sleep_3.h"
#include "pet_sleep_4.h"
#include "pet_sleep_5.h"
#include "pet_walk_1.h"
#include "pet_walk_2.h"
#include "pet_walk_3.h"
#include "pet_walk_4.h"

#define PET_SPRITE_W  56
#define PET_SPRITE_H  56

typedef enum { MOT_IDLE, MOT_MOVEFORWARD, MOT_SPRINTFORWARD, MOT_MOVEUP, MOT_SPRINTUP } pet_mot_t;

typedef struct {
    const char *name;                        /* 显示标签 (动作名大写) */
    const lv_image_dsc_t *const *frames;    /* 帧指针表 */
    uint8_t  n_frames;
    uint16_t frame_ms;     /* 换帧周期 (ms) */
    uint16_t hold_ms;      /* 静止持续; 0 = 移动到撞墙 */
    pet_mot_t motion;      /* 运动学类型 */
    uint8_t  step_px;      /* 每 tick 水平位移 */
    int8_t   hop_max;      /* 垂直跳跃幅度 (0 = 不跳) */
    bool     stationary;   /* true = 原地不动 */
} pet_action_t;

static const lv_image_dsc_t *const pet_frames_groom[] = { &pet_groom_1, &pet_groom_2, &pet_groom_3 };
static const lv_image_dsc_t *const pet_frames_jump[] = { &pet_jump_1, &pet_jump_2, &pet_jump_3, &pet_jump_4 };
static const lv_image_dsc_t *const pet_frames_sleep[] = { &pet_sleep_1, &pet_sleep_2, &pet_sleep_3, &pet_sleep_4, &pet_sleep_5 };
static const lv_image_dsc_t *const pet_frames_walk[] = { &pet_walk_1, &pet_walk_2, &pet_walk_3, &pet_walk_4 };

static const pet_action_t pet_actions[] = {
  /* GROOM */ { "GROOM", pet_frames_groom, 3, 350, 3000, MOT_IDLE, 0, 0, true },
  /* JUMP */ { "JUMP", pet_frames_jump, 4, 110, 0, MOT_SPRINTUP, 2, 12, false },
  /* SLEEP */ { "SLEEP", pet_frames_sleep, 5, 350, 3000, MOT_IDLE, 0, 0, true },
  /* WALK */ { "WALK", pet_frames_walk, 4, 130, 0, MOT_MOVEFORWARD, 1, 0, false },
};

#define PET_ACT_COUNT  4

/* 静止动作下标池 (撞墙后随机休息) */
static const uint8_t pet_rest_pool[] = { 0, 2 };
#define PET_REST_N  (sizeof(pet_rest_pool) / sizeof(pet_rest_pool[0]))

/* 移动动作下标池 (休息完恢复移动 / 移动中随机切换) */
static const uint8_t pet_move_pool[] = { 1, 3 };
#define PET_MOVE_N  (sizeof(pet_move_pool) / sizeof(pet_move_pool[0]))

#endif /* PET_MANIFEST_H */
