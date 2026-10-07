#pragma once

#include "eevee_types.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EEVEE_PAGE_BADGE = 0,    // 0: 电子工牌主卡
    EEVEE_PAGE_TASKS = 1,    // 1: 待办审批中心
    EEVEE_PAGE_RECORDS = 2,  // 2: 业务数据看板
    EEVEE_PAGE_QR = 3,       // 3: 电子名片二维码
    EEVEE_PAGE_SETTINGS = 4, // 4: 设备设置与配网
    EEVEE_PAGE_COUNT = 5,
} eevee_ui_page_t;

/**
 * 初始化并构建工牌全套 UI (状态栏 + 4 大核心视图卡片)
 */
void eevee_badge_ui_init(void);

/**
 * 切换到指定页面
 */
void eevee_badge_ui_set_page(eevee_ui_page_t page);

/**
 * 切换到下一页
 */
void eevee_badge_ui_next_page(void);

/**
 * 切换到上一页
 */
void eevee_badge_ui_prev_page(void);

/**
 * 获取当前展示页面
 */
eevee_ui_page_t eevee_badge_ui_get_page(void);

/**
 * 更新顶部全局状态栏 (Wi-Fi, 电量)
 */
void eevee_badge_ui_update_status(bool wifi_connected, int battery_soc);

/**
 * 刷新个人工牌卡面信息
 */
void eevee_badge_ui_update_profile(const eevee_profile_t *profile);

/**
 * 刷新待办审批工单信息
 */
void eevee_badge_ui_update_task(const eevee_task_t *task);

/**
 * 刷新业务看板指标信息
 */
void eevee_badge_ui_update_record(const eevee_record_t *record);

/**
 * 刷新名片二维码跳转 URL (内网员工主页/名片页)
 */
void eevee_badge_ui_update_qr_url(const char *url);

/**
 * 刷新设备设置页面信息 (Wi-Fi 状态, 服务端地址, 令牌状态)
 */
void eevee_badge_ui_update_settings(const char *wifi_ssid, bool connected,
                                    const char *server_url, bool has_token);

/**
 * 弹出简短浮层提示 (如 "审批成功"、"上报完成")
 */
void eevee_badge_ui_show_toast(const char *text, uint32_t color_hex);

/**
 * 显示或隐藏热点配网全屏视图
 */
void eevee_badge_ui_show_prov_view(bool visible, const char *ap_ssid, const char *web_url);

/**
 * 判断当前是否处于配网视图
 */
bool eevee_badge_ui_is_prov_view_visible(void);

#ifdef __cplusplus
}
#endif
