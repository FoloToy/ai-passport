#include "eevee_parser.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

static void safe_copy_str(char *dest, size_t dest_size, const cJSON *item)
{
    if (!dest || dest_size == 0) return;
    dest[0] = '\0';
    if (cJSON_IsString(item) && item->valuestring) {
        strncpy(dest, item->valuestring, dest_size - 1);
        dest[dest_size - 1] = '\0';
    }
}

bool eevee_parse_me(const char *json_str, eevee_profile_t *out_profile)
{
    if (!json_str || !out_profile) return false;
    memset(out_profile, 0, sizeof(*out_profile));

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return false;

    cJSON *ok_item = cJSON_GetObjectItem(root, "ok");
    if (!cJSON_IsBool(ok_item) || !cJSON_IsTrue(ok_item)) {
        cJSON_Delete(root);
        return false;
    }

    safe_copy_str(out_profile->id, sizeof(out_profile->id),
                  cJSON_GetObjectItem(root, "id"));
    safe_copy_str(out_profile->username, sizeof(out_profile->username),
                  cJSON_GetObjectItem(root, "username"));
    safe_copy_str(out_profile->display_name, sizeof(out_profile->display_name),
                  cJSON_GetObjectItem(root, "displayName"));
    if (out_profile->display_name[0] == '\0') {
        safe_copy_str(out_profile->display_name, sizeof(out_profile->display_name),
                      cJSON_GetObjectItem(root, "name"));
    }
    if (out_profile->display_name[0] == '\0') {
        safe_copy_str(out_profile->display_name, sizeof(out_profile->display_name),
                      cJSON_GetObjectItem(root, "username"));
    }
    safe_copy_str(out_profile->dept, sizeof(out_profile->dept),
                  cJSON_GetObjectItem(root, "dept"));
    safe_copy_str(out_profile->role, sizeof(out_profile->role),
                  cJSON_GetObjectItem(root, "role"));

    out_profile->valid = (out_profile->id[0] != '\0');
    cJSON_Delete(root);
    return out_profile->valid;
}

bool eevee_parse_tasks(const char *json_str, eevee_task_t *out_task)
{
    if (!json_str || !out_task) return false;
    memset(out_task, 0, sizeof(*out_task));

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return false;

    cJSON *count_item = cJSON_GetObjectItem(root, "count");
    if (cJSON_IsNumber(count_item)) {
        out_task->total_count = count_item->valueint;
    }

    cJSON *offset_item = cJSON_GetObjectItem(root, "offset");
    if (cJSON_IsNumber(offset_item)) {
        out_task->offset = offset_item->valueint;
    }

    cJSON *latest = cJSON_GetObjectItem(root, "latest");
    if (!latest || cJSON_IsNull(latest)) {
        out_task->has_task = false;
        cJSON_Delete(root);
        return true;
    }

    out_task->has_task = true;
    safe_copy_str(out_task->app_id, sizeof(out_task->app_id),
                  cJSON_GetObjectItem(latest, "appId"));
    safe_copy_str(out_task->record_id, sizeof(out_task->record_id),
                  cJSON_GetObjectItem(latest, "recordId"));
    safe_copy_str(out_task->title, sizeof(out_task->title),
                  cJSON_GetObjectItem(latest, "title"));
    safe_copy_str(out_task->state, sizeof(out_task->state),
                  cJSON_GetObjectItem(latest, "state"));
    safe_copy_str(out_task->primary_action_id, sizeof(out_task->primary_action_id),
                  cJSON_GetObjectItem(latest, "actionId"));
    safe_copy_str(out_task->primary_action_name, sizeof(out_task->primary_action_name),
                  cJSON_GetObjectItem(latest, "actionName"));

    cJSON *actions = cJSON_GetObjectItem(latest, "actions");
    if (cJSON_IsArray(actions)) {
        int arr_size = cJSON_GetArraySize(actions);
        for (int i = 0; i < arr_size && i < EEVEE_MAX_ACTIONS; ++i) {
            cJSON *action_item = cJSON_GetArrayItem(actions, i);
            if (!action_item) continue;

            safe_copy_str(out_task->actions[i].id, sizeof(out_task->actions[i].id),
                          cJSON_GetObjectItem(action_item, "id"));
            safe_copy_str(out_task->actions[i].name, sizeof(out_task->actions[i].name),
                          cJSON_GetObjectItem(action_item, "name"));
            out_task->action_count++;

            // 智能识别同意与驳回动作
            if (strstr(out_task->actions[i].name, "同意") ||
                strstr(out_task->actions[i].id, "approve") ||
                strstr(out_task->actions[i].id, "pass")) {
                strncpy(out_task->primary_action_id, out_task->actions[i].id,
                        sizeof(out_task->primary_action_id) - 1);
                strncpy(out_task->primary_action_name, out_task->actions[i].name,
                        sizeof(out_task->primary_action_name) - 1);
            } else if (strstr(out_task->actions[i].name, "驳回") ||
                       strstr(out_task->actions[i].id, "reject")) {
                strncpy(out_task->reject_action_id, out_task->actions[i].id,
                        sizeof(out_task->reject_action_id) - 1);
                strncpy(out_task->reject_action_name, out_task->actions[i].name,
                        sizeof(out_task->reject_action_name) - 1);
            }
        }
    }

    cJSON_Delete(root);
    return true;
}

bool eevee_parse_notifications(const char *json_str, eevee_notification_t *out_notif)
{
    if (!json_str || !out_notif) return false;
    memset(out_notif, 0, sizeof(*out_notif));

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return false;

    cJSON *count_item = cJSON_GetObjectItem(root, "count");
    if (cJSON_IsNumber(count_item)) {
        out_notif->unread_count = count_item->valueint;
    }

    cJSON *latest = cJSON_GetObjectItem(root, "latest");
    if (latest && !cJSON_IsNull(latest)) {
        out_notif->has_latest = true;
        safe_copy_str(out_notif->id, sizeof(out_notif->id),
                      cJSON_GetObjectItem(latest, "id"));
        safe_copy_str(out_notif->title, sizeof(out_notif->title),
                      cJSON_GetObjectItem(latest, "title"));
        safe_copy_str(out_notif->content, sizeof(out_notif->content),
                      cJSON_GetObjectItem(latest, "content"));
    }

    cJSON_Delete(root);
    return true;
}

bool eevee_parse_latest_record(const char *json_str, eevee_record_t *out_record)
{
    if (!json_str || !out_record) return false;
    memset(out_record, 0, sizeof(*out_record));

    cJSON *root = cJSON_Parse(json_str);
    if (!root) return false;

    cJSON *num_item = cJSON_GetObjectItem(root, "recordNumber");
    if (cJSON_IsNumber(num_item)) {
        out_record->record_number = num_item->valueint;
        out_record->valid = true;
    }

    cJSON *values = cJSON_GetObjectItem(root, "values");
    if (values && cJSON_IsObject(values)) {
        size_t written = 0;
        out_record->summary[0] = '\0';
        cJSON *child = values->child;
        while (child && written < sizeof(out_record->summary) - 1) {
            char line_buf[64] = {0};
            const char *key = child->string ? child->string : "item";

            if (strcmp(key, "temperature") == 0 && cJSON_IsNumber(child)) {
                snprintf(line_buf, sizeof(line_buf), "温度: %.1f C\n", child->valuedouble);
            } else if (strcmp(key, "humidity") == 0 && cJSON_IsNumber(child)) {
                snprintf(line_buf, sizeof(line_buf), "湿度: %.1f %%\n", child->valuedouble);
            } else if (strcmp(key, "battery_volt") == 0 && cJSON_IsNumber(child)) {
                snprintf(line_buf, sizeof(line_buf), "电压: %.2f V\n", child->valuedouble);
            } else {
                char val_buf[32] = {0};
                if (cJSON_IsNumber(child)) {
                    snprintf(val_buf, sizeof(val_buf), "%.1f", child->valuedouble);
                } else if (cJSON_IsString(child) && child->valuestring) {
                    snprintf(val_buf, sizeof(val_buf), "%s", child->valuestring);
                } else if (cJSON_IsBool(child)) {
                    snprintf(val_buf, sizeof(val_buf), "%s", cJSON_IsTrue(child) ? "true" : "false");
                }
                if (val_buf[0] != '\0') {
                    snprintf(line_buf, sizeof(line_buf), "%s: %s\n", key, val_buf);
                }
            }

            if (line_buf[0] != '\0') {
                int n = snprintf(out_record->summary + written,
                                 sizeof(out_record->summary) - written,
                                 "%s", line_buf);
                if (n > 0) written += (size_t)n;
            }
            child = child->next;
        }
    }

    cJSON_Delete(root);
    return out_record->valid;
}

bool eevee_build_task_action_body(const char *app_id, const char *record_id,
                                  const char *action_id, const char *comment,
                                  char *out_buf, size_t buf_size)
{
    if (!app_id || !record_id || !action_id || !out_buf || buf_size == 0) {
        return false;
    }
    const char *actual_comment = (comment && comment[0]) ? comment : "AI Passport 物理按键确认";
    int n = snprintf(out_buf, buf_size,
                     "{\"appId\":\"%s\",\"recordId\":\"%s\",\"actionId\":\"%s\",\"comment\":\"%s\"}",
                     app_id, record_id, action_id, actual_comment);
    return (n > 0 && (size_t)n < buf_size);
}

bool eevee_build_record_body(float temp, float hum, float battery_volt,
                             char *out_buf, size_t buf_size)
{
    if (!out_buf || buf_size == 0) return false;
    int n = snprintf(out_buf, buf_size,
                     "{\"temperature\":%.1f,\"humidity\":%.1f,\"battery_volt\":%.2f}",
                     temp, hum, battery_volt);
    return (n > 0 && (size_t)n < buf_size);
}

bool eevee_build_mark_read_body(const char *notif_id_or_all,
                                char *out_buf, size_t buf_size)
{
    if (!out_buf || buf_size == 0) return false;
    const char *id = (notif_id_or_all && notif_id_or_all[0]) ? notif_id_or_all : "all";
    int n = snprintf(out_buf, buf_size, "{\"id\":\"%s\"}", id);
    return (n > 0 && (size_t)n < buf_size);
}
