/* main/cat/cat_sprites.h —— 小猫精灵资源总表
 *
 * 所有帧由 tools/png2lvgl.py 从 cats/<action_frame_dir>.png 一站式生成：
 *     python3 tools/prep_cats.py          # 默认 --size 56
 *
 * 文件命名约定: action_frame_dir.png (例 walk_3_l.png)
 *   - dir == 'l' : png2lvgl 自动水平镜像, 归一为朝右
 *   - dir == 'r' / 缺省 : 原样 (朝右)
 *   - 符号名: cat_{action}_{frame} (去掉方向后缀)
 *
 * AI 水印删除遗留的"空像素"(透明) 区域:
 *   - alpha>0 bbox 自动忽略, 不会把空洞误判为猫
 *   - 输出保持透明 (alpha=0), 不填成黑/白块
 *
 * 规格: 56x56 ARGB8888, 每帧 12544 字节 Flash, 16 帧 = 196 KB
 */
#ifndef CAT_SPRITES_H
#define CAT_SPRITES_H

#include "cat_walk_1.h"
#include "cat_walk_2.h"
#include "cat_walk_3.h"
#include "cat_walk_4.h"

#include "cat_jump_1.h"
#include "cat_jump_2.h"
#include "cat_jump_3.h"
#include "cat_jump_4.h"

#include "cat_sleep_1.h"
#include "cat_sleep_2.h"
#include "cat_sleep_3.h"
#include "cat_sleep_4.h"
#include "cat_sleep_5.h"

#include "cat_groom_1.h"
#include "cat_groom_2.h"
#include "cat_groom_3.h"

/* 原始精灵尺寸 (png2lvgl 生成时的像素尺寸) */
#define CAT_SPRITE_W  56
#define CAT_SPRITE_H  56

#endif /* CAT_SPRITES_H */