// main/finder_view.h —— Pocket Finder 的 LVGL 界面（三页）。
//
// 这是【自建的界面】，不是基线 demo 的测试外壳：不使用 ui_pixel_*，页面、布局、
// 配色与交互全部按 PRD §6/§9 重新设计（AGENTS.md 的强制 UI 重设计规则）。
//
// 全部调用都必须在 LVGL 上下文内（本应用在 lv_timer 回调里驱动）。
//
// 字形安全：sdkconfig 只启用了 Montserrat 14/20，其字形覆盖不保证包含箭头、破折号
// 等非 ASCII 符号，缺字形会渲染成空白框。因此本界面【只用 ASCII】。PRD §7 里的
// "↑/↓ 趋势箭头"因此落地为等义的英文字词（STRONGER / WEAKER / STEADY）。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "finder_table.h"
#include "finder_ui.h"

// 建立并载入指定页面。会先删除上一个页面，以把 LVGL 对象数控制在内存预算内。
void finder_view_show(finder_page_t page);

// 依据状态与设备表刷新当前页面。notice 为 NULL 或空串表示不显示瞬时提示。
void finder_view_update(const finder_ui_t *ui, const finder_table_t *table,
                        int64_t now_ms, const char *notice);

// 删除当前页面。退出应用时调用，先于释放任务与定时器。
void finder_view_delete(void);

// 当前是否显示了某个页面（供调用方判断是否需要重建）。
bool finder_view_active(void);
