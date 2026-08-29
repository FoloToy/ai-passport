// main/passport_store.h —— NVS 封装：命名空间 "smpass" 的唯一读写模块。
//
// key 布局见《创智学员 Passport 实现文档》7.2 节。NVS key 上限 15 字符，
// 故 "last_checkin_date" 实存为 "last_ckin"（其他 key 与文档一致）。
//
// 安全红线：secret 只经本模块进出 NVS；任何日志不得打印其内容。
#pragma once

#include "esp_err.h"
#include "passport_core.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// NVS key（命名空间固定为 "smpass"）
#define PASSPORT_KEY_PID         "pid"
#define PASSPORT_KEY_SECRET      "secret"       // blob(32B)，禁止打印
#define PASSPORT_KEY_WIFI1_SSID  "wifi1_ssid"
#define PASSPORT_KEY_WIFI1_PASS  "wifi1_pass"
#define PASSPORT_KEY_WIFI2_SSID  "wifi2_ssid"
#define PASSPORT_KEY_WIFI2_PASS  "wifi2_pass"
#define PASSPORT_KEY_LAB_SSID    "lab_ssid"
#define PASSPORT_KEY_PP_TOKEN    "pp_token"
#define PASSPORT_KEY_BALANCE     "balance"      // i32
#define PASSPORT_KEY_BALANCE_TS  "balance_ts"   // str "MM-DD HH:MM"
#define PASSPORT_KEY_NAME        "name"
#define PASSPORT_KEY_STUDENT_ID  "student_id"
#define PASSPORT_KEY_AVATAR_URL  "avatar_url"
#define PASSPORT_KEY_IN_LAB      "in_lab"       // u8
#define PASSPORT_KEY_TIME_VALID  "time_valid"   // u8
#define PASSPORT_KEY_LAST_CHECKIN "last_ckin"   // str YYYY-MM-DD（Asia/Shanghai）

// 校区网 SSID 缺省值（心跳门控，见实现文档 7.5 节）
#define PASSPORT_DEFAULT_LAB_SSID "SparkMinds_IoT"

// 初始化 NVS（不擦除任何分区；失败只返回错误）。
esp_err_t passport_store_init(void);

// 是否已产线激活（pid 与 secret 均存在）
bool passport_store_is_provisioned(void);

// 通用字符串读写。未找到返回 ESP_ERR_NVS_NOT_FOUND。
esp_err_t passport_store_get_string(const char *key, char *buf, size_t len);
esp_err_t passport_store_set_string(const char *key, const char *val);

esp_err_t passport_store_get_i32(const char *key, int32_t *out);
esp_err_t passport_store_set_i32(const char *key, int32_t val);

esp_err_t passport_store_get_u8(const char *key, uint8_t *out);
esp_err_t passport_store_set_u8(const char *key, uint8_t val);

// 读取设备密钥（仅 passport_net / passport_app 产线流程使用）。
esp_err_t passport_store_get_secret(uint8_t out[PASSPORT_SECRET_LEN]);

// 产线写入 pid + secret（覆盖写；调用方负责"先 erase 再 provision"的门禁）。
esp_err_t passport_store_provision(const char *pid,
                                   const uint8_t secret[PASSPORT_SECRET_LEN]);

// 恢复出厂：只擦除 "smpass" 命名空间，绝不动整片 NVS（出厂预配数据红线）。
esp_err_t passport_store_erase_all(void);
