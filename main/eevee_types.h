#pragma once

#include <stdbool.h>
#include <stdint.h>

#define EEVEE_MAX_STRING_LEN     64
#define EEVEE_MAX_TITLE_LEN      96
#define EEVEE_MAX_CONTENT_LEN    160
#define EEVEE_MAX_ACTIONS        4

/**
 * 员工工牌个人档案数据模型
 * 映射至 GET /api/iot/me
 */
typedef struct {
    bool valid;
    char id[EEVEE_MAX_STRING_LEN];           // 员工 ID，如 usr_9a4f6e1b...
    char username[EEVEE_MAX_STRING_LEN];     // 登录名，如 zhangsan
    char display_name[EEVEE_MAX_STRING_LEN]; // 中文姓名，如 张三
    char dept[EEVEE_MAX_STRING_LEN];         // 部门名称，如 研发部
    char role[EEVEE_MAX_STRING_LEN];         // 职位名称，如 高级工程师
} eevee_profile_t;

/**
 * 工作流可用动作项
 */
typedef struct {
    char id[32];   // 动作 ID，如 act_approve, act_reject
    char name[32]; // 动作名称，如 同意, 驳回
} eevee_action_item_t;

/**
 * 待办审批工单数据模型
 * 映射至 GET /api/iot/tasks?offset=N
 */
typedef struct {
    bool has_task;
    int total_count;                         // 待办总数
    int offset;                              // 当前查看序号 (0-based)
    char app_id[EEVEE_MAX_STRING_LEN];       // 业务应用 ID，如 app_purchase
    char record_id[EEVEE_MAX_STRING_LEN];    // 记录 ID，如 rec_019a88bc
    char title[EEVEE_MAX_TITLE_LEN];         // 工单标题，如 购买备件采购单 #12
    char state[EEVEE_MAX_STRING_LEN];        // 节点状态，如 主管审批
    char primary_action_id[32];              // 首选动作 ID (通常为同意)
    char primary_action_name[32];            // 首选动作名
    char reject_action_id[32];               // 驳回动作 ID
    char reject_action_name[32];             // 驳回动作名
    int action_count;
    eevee_action_item_t actions[EEVEE_MAX_ACTIONS];
} eevee_task_t;

/**
 * 站内通知与告警数据模型
 * 映射至 GET /api/iot/notifications/unread
 */
typedef struct {
    int unread_count;
    bool has_latest;
    char id[EEVEE_MAX_STRING_LEN];
    char title[EEVEE_MAX_TITLE_LEN];
    char content[EEVEE_MAX_CONTENT_LEN];
} eevee_notification_t;

/**
 * 业务数据看板记录模型
 * 映射至 GET /api/iot/apps/:appId/records/latest
 */
typedef struct {
    bool valid;
    int record_number;
    char summary[EEVEE_MAX_CONTENT_LEN];     // 核心指标与键值对摘要
} eevee_record_t;

/**
 * 工牌持久化配置参数模型
 */
typedef struct {
    char wifi_ssid[33];
    char wifi_pass[65];
    char base_url[129];                      // 例如 http://192.168.1.100:3001
    char pat_token[97];                      // 例如 usr_9a4f6e1b7c3d2e5a...
    char app_id[48];                         // 绑定的业务看板应用 ID (如 UUID: 36 字符)
    int poll_interval_sec;                   // 后台周期轮询间隔 (秒，默认 45)
} eevee_config_t;
