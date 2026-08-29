// main/passport_core.h —— 创智学员 Passport 纯逻辑层。
//
// 动态码计算、URL 构造、auth 签名、窗口换算、像素头像派生。
// 纯 C11、零 ESP-IDF/LVGL 依赖，host 可测（tests/test_passport_core.c）。
// 契约来源：《创智学员 Passport 实现文档》第 6 节，逐字冻结，不得自行发挥。
#pragma once

#include <stddef.h>
#include <stdint.h>

// 契约常量（实现文档 6.1 节）
#define PASSPORT_SECRET_LEN     32   // 设备密钥，32 字节
#define PASSPORT_CODE_LEN       6    // 动态码位数（十进制，左补零）
#define PASSPORT_SIG_HEX_LEN    64   // auth 签名 hex 小写长度
#define PASSPORT_WINDOW_SECONDS 30   // 二维码窗口周期
#define PASSPORT_AVATAR_SIZE    8    // 像素头像边长（8x8，左右对称）

// 时钟有效下限：2026-01-01 00:00:00 UTC。早于该时间视为未校时。
#define PASSPORT_MIN_VALID_TS   1767225600U

// 登录二维码落地页（实现文档 6.1 节）
#define PASSPORT_QR_BASE_URL "https://learn.sparkminds.cn/passport-login"

// window = ts / 30（整数除法）
uint32_t passport_window(uint32_t ts);

// 系统时钟是否已完成校时（可用于生成二维码）
int passport_time_is_valid(uint32_t ts);

// 计算 6 位动态码：
//   msg = "qr:" + pid + ":" + window
//   mac = HMAC-SHA256(secret, msg)
//   c   = uint32_be(mac[0..4]) % 1000000，左补零至 6 位
// out_code 至少 PASSPORT_CODE_LEN+1 字节，输出 NUL 结尾数字串。
void passport_qr_code(const uint8_t secret[PASSPORT_SECRET_LEN],
                      const char *pid, uint32_t ts,
                      char out_code[PASSPORT_CODE_LEN + 1]);

// 构造登录二维码 URL：
//   PASSPORT_QR_BASE_URL "?pid=" pid "&ts=" ts "&c=" code
// 返回写入长度（不含 NUL）；buf 不足返回 -1。
int passport_qr_url(const char *pid, uint32_t ts,
                    const char code[PASSPORT_CODE_LEN + 1],
                    char *buf, size_t buflen);

// 计算设备会话 auth 签名：
//   msg = "auth:" + pid + ":" + ts
//   sig = hex( HMAC-SHA256(secret, msg) )，64 字符小写
// out_sig_hex 至少 PASSPORT_SIG_HEX_LEN+1 字节，NUL 结尾。
void passport_auth_sig(const uint8_t secret[PASSPORT_SECRET_LEN],
                       const char *pid, uint32_t ts,
                       char out_sig_hex[PASSPORT_SIG_HEX_LEN + 1]);

// 学号派生像素头像（R10a）：对 student_id 做确定性哈希，生成 8x8 左右对称
// 图案。out[r][c] ∈ {0,1}，1 = 着品牌色。同输入必同输出，host 可测。
void passport_avatar_pattern(const char *student_id,
                             uint8_t out[PASSPORT_AVATAR_SIZE][PASSPORT_AVATAR_SIZE]);
