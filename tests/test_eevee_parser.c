#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "eevee_parser.h"

static void test_parse_me(void)
{
    const char *json = "{\"ok\":true,\"id\":\"usr_9a4f6e1b7c3d2e5a\","
                       "\"username\":\"zhangsan\",\"displayName\":\"张三\","
                       "\"dept\":\"研发部\",\"role\":\"高级工程师\"}";
    eevee_profile_t profile;
    bool ok = eevee_parse_me(json, &profile);
    assert(ok);
    assert(profile.valid);
    assert(strcmp(profile.id, "usr_9a4f6e1b7c3d2e5a") == 0);
    assert(strcmp(profile.username, "zhangsan") == 0);
    assert(strcmp(profile.display_name, "张三") == 0);
    assert(strcmp(profile.dept, "研发部") == 0);
    assert(strcmp(profile.role, "高级工程师") == 0);

    // 回退情况：缺失 displayName 时回退到 name
    const char *json_name = "{\"ok\":true,\"id\":\"usr_1\",\"name\":\"李四\",\"username\":\"lisi\"}";
    assert(eevee_parse_me(json_name, &profile));
    assert(strcmp(profile.display_name, "李四") == 0);

    // 回退情况：缺失 displayName 与 name 时回退到 username
    const char *json_user = "{\"ok\":true,\"id\":\"usr_2\",\"username\":\"wangwu\"}";
    assert(eevee_parse_me(json_user, &profile));
    assert(strcmp(profile.display_name, "wangwu") == 0);

    // 失败情况
    const char *fail_json = "{\"ok\":false}";
    assert(!eevee_parse_me(fail_json, &profile));
}

static void test_parse_tasks(void)
{
    const char *json_with_task =
        "{\"count\":2,\"offset\":0,\"latest\":{"
        "\"appId\":\"app_purchase\",\"recordId\":\"rec_019a88bc\","
        "\"title\":\"购买备件采购单 #12\",\"state\":\"主管审批\","
        "\"actionId\":\"act_approve\",\"actionName\":\"同意\","
        "\"actions\":["
        "{\"id\":\"act_approve\",\"name\":\"同意\"},"
        "{\"id\":\"act_reject\",\"name\":\"驳回\"}"
        "]}}";

    eevee_task_t task;
    bool ok = eevee_parse_tasks(json_with_task, &task);
    assert(ok);
    assert(task.has_task);
    assert(task.total_count == 2);
    assert(task.offset == 0);
    assert(strcmp(task.app_id, "app_purchase") == 0);
    assert(strcmp(task.record_id, "rec_019a88bc") == 0);
    assert(strcmp(task.title, "购买备件采购单 #12") == 0);
    assert(strcmp(task.state, "主管审批") == 0);
    assert(strcmp(task.primary_action_id, "act_approve") == 0);
    assert(strcmp(task.primary_action_name, "同意") == 0);
    assert(strcmp(task.reject_action_id, "act_reject") == 0);
    assert(strcmp(task.reject_action_name, "驳回") == 0);

    // 无待办情况
    const char *json_empty = "{\"count\":0,\"offset\":0,\"latest\":null}";
    assert(eevee_parse_tasks(json_empty, &task));
    assert(!task.has_task);
    assert(task.total_count == 0);
}

static void test_parse_notifications(void)
{
    const char *json = "{\"count\":1,\"latest\":{"
                       "\"id\":\"notif_019a7788\","
                       "\"title\":\"高温告警\","
                       "\"content\":\"车间温度超过 35 度\"}}";
    eevee_notification_t notif;
    bool ok = eevee_parse_notifications(json, &notif);
    assert(ok);
    assert(notif.unread_count == 1);
    assert(notif.has_latest);
    assert(strcmp(notif.id, "notif_019a7788") == 0);
    assert(strcmp(notif.title, "高温告警") == 0);
    assert(strcmp(notif.content, "车间温度超过 35 度") == 0);
}

static void test_parse_latest_record(void)
{
    const char *json = "{\"recordNumber\":108,\"values\":{"
                       "\"temp_threshold\":30.0,\"fan_mode\":\"auto\"}}";
    eevee_record_t record;
    bool ok = eevee_parse_latest_record(json, &record);
    assert(ok);
    assert(record.valid);
    assert(record.record_number == 108);
    assert(strstr(record.summary, "temp_threshold") != NULL);
    assert(strstr(record.summary, "fan_mode: auto") != NULL);

    const char *json_iot = "{\"recordNumber\":13,\"values\":{"
                           "\"temperature\":26.5,\"humidity\":60.0,\"battery_volt\":3.90}}";
    assert(eevee_parse_latest_record(json_iot, &record));
    assert(record.valid);
    assert(record.record_number == 13);
    assert(strstr(record.summary, "温度: 26.5 C") != NULL);
    assert(strstr(record.summary, "湿度: 60.0 %") != NULL);
    assert(strstr(record.summary, "电压: 3.90 V") != NULL);
}

static void test_builders(void)
{
    char buf[256];
    assert(eevee_build_task_action_body("app_1", "rec_2", "act_pass", "OK",
                                       buf, sizeof(buf)));
    assert(strstr(buf, "\"appId\":\"app_1\"") != NULL);
    assert(strstr(buf, "\"recordId\":\"rec_2\"") != NULL);
    assert(strstr(buf, "\"actionId\":\"act_pass\"") != NULL);
    assert(strstr(buf, "\"comment\":\"OK\"") != NULL);

    assert(eevee_build_record_body(26.5f, 62.0f, 3.84f, buf, sizeof(buf)));
    assert(strstr(buf, "\"temperature\":26.5") != NULL);
    assert(strstr(buf, "\"humidity\":62.0") != NULL);
    assert(strstr(buf, "\"battery_volt\":3.84") != NULL);

    assert(eevee_build_mark_read_body("notif_123", buf, sizeof(buf)));
    assert(strcmp(buf, "{\"id\":\"notif_123\"}") == 0);
}

int main(void)
{
    test_parse_me();
    test_parse_tasks();
    test_parse_notifications();
    test_parse_latest_record();
    test_builders();
    printf("test_eevee_parser: ALL PASS\n");
    return 0;
}
