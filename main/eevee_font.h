#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 初始化并加载 Noto CJK 4bpp 紧凑多语言字库 (零 SRAM 内存开销)
 */
void eevee_font_init(void);

/**
 * 获取正文/卡面使用的默认字体 (支持中日英及工作流状态符号)
 */
const lv_font_t *eevee_font_get(void);

/**
 * 获取姓名/大标题使用的字体
 */
const lv_font_t *eevee_font_title_get(void);

#ifdef __cplusplus
}
#endif
