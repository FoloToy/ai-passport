#pragma once

#include "eevee_types.h"
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 解析 GET /api/iot/me 响应
 * 示例:
 * {
 *   "ok": true,
 *   "id": "usr_9a4f6e1b7c3d2e5a",
 *   "username": "zhangsan",
 *   "displayName": "张三",
 *   "dept": "研发部",
 *   "role": "高级工程师"
 * }
 */
bool eevee_parse_me(const char *json_str, eevee_profile_t *out_profile);

/**
 * 解析 GET /api/iot/tasks 响应
 * 示例:
 * {
 *   "count": 2,
 *   "offset": 0,
 *   "latest": {
 *     "appId": "app_purchase",
 *     "recordId": "rec_019a88bc",
 *     "title": "购买备件采购单 #12",
 *     "state": "主管审批",
 *     "actionId": "act_approve",
 *     "actionName": "同意",
 *     "actions": [
 *       { "id": "act_approve", "name": "同意" },
 *       { "id": "act_reject", "name": "驳回" }
 *     ]
 *   }
 * }
 */
bool eevee_parse_tasks(const char *json_str, eevee_task_t *out_task);

/**
 * 解析 GET /api/iot/notifications/unread 响应
 * 示例:
 * {
 *   "count": 1,
 *   "latest": {
 *     "id": "notif_019a7788",
 *     "title": "高温告警",
 *     "content": "车间温度超过 35 度"
 *   }
 * }
 */
bool eevee_parse_notifications(const char *json_str, eevee_notification_t *out_notif);

/**
 * 解析 GET /api/iot/apps/:appId/records/latest 响应
 * 示例:
 * {
 *   "recordNumber": 108,
 *   "values": {
 *     "temp_threshold": 30.0,
 *     "fan_mode": "auto"
 *   }
 * }
 */
bool eevee_parse_latest_record(const char *json_str, eevee_record_t *out_record);

/**
 * 构建 POST /api/iot/tasks/action 请求体 (写入调用方提供的缓冲，避免堆分配)
 */
bool eevee_build_task_action_body(const char *app_id, const char *record_id,
                                  const char *action_id, const char *comment,
                                  char *out_buf, size_t buf_size);

/**
 * 构建 POST /api/iot/apps/:appId/records 传感器/状态上报请求体
 */
bool eevee_build_record_body(float temp, float hum, float battery_volt,
                             char *out_buf, size_t buf_size);

/**
 * 构建 POST /api/iot/notifications/read 请求体
 */
bool eevee_build_mark_read_body(const char *notif_id_or_all,
                                char *out_buf, size_t buf_size);

#ifdef __cplusplus
}
#endif
