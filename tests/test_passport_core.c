// tests/test_passport_core.c —— passport_core host 测试。
//
// 对拍《创智学员 Passport 实现文档》6.4 节公开测试向量（三方公契，
// 与《api 改进文档》第 9 节同套向量），外加像素头像稳定性与对称性检查。
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "passport_core.h"

// 公开测试向量（实现文档 6.4 节；非真实设备密钥，允许入库）
static const uint8_t TEST_SECRET[PASSPORT_SECRET_LEN] = {
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
    0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77,
    0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff,
};
static const char TEST_PID[] = "SM-2026-083";

static void test_window(void)
{
    assert(passport_window(1770000000U) == 59000000U);
    assert(passport_window(1770000030U) == 59000001U);
    assert(passport_window(1770000123U) == 59000004U);
    assert(passport_window(0U) == 0U);
    assert(passport_window(29U) == 0U);
    assert(passport_window(30U) == 1U);
}

static void test_time_valid(void)
{
    assert(passport_time_is_valid(0U) == 0);
    assert(passport_time_is_valid(1767225599U) == 0);  // 2026-01-01 前一秒
    assert(passport_time_is_valid(1767225600U) == 1);  // 2026-01-01 00:00:00 UTC
    assert(passport_time_is_valid(1770000000U) == 1);
}

static void test_qr_codes(void)
{
    char code[PASSPORT_CODE_LEN + 1];

    passport_qr_code(TEST_SECRET, TEST_PID, 1770000000U, code);
    assert(strcmp(code, "465062") == 0);

    passport_qr_code(TEST_SECRET, TEST_PID, 1770000030U, code);
    assert(strcmp(code, "491270") == 0);

    passport_qr_code(TEST_SECRET, TEST_PID, 1770000123U, code);
    assert(strcmp(code, "841578") == 0);
}

static void test_qr_url(void)
{
    char code[PASSPORT_CODE_LEN + 1];
    char url[128];

    passport_qr_code(TEST_SECRET, TEST_PID, 1770000000U, code);
    int len = passport_qr_url(TEST_PID, 1770000000U, code, url, sizeof(url));
    assert(len == 81);
    assert(strcmp(url,
        "https://learn.sparkminds.cn/passport-login"
        "?pid=SM-2026-083&ts=1770000000&c=465062") == 0);

    // 缓冲不足必须失败，不得截断输出半条 URL
    char small[40];
    assert(passport_qr_url(TEST_PID, 1770000000U, code, small, sizeof(small)) == -1);
}

static void test_auth_sig(void)
{
    char sig[PASSPORT_SIG_HEX_LEN + 1];
    passport_auth_sig(TEST_SECRET, TEST_PID, 1770000000U, sig);
    assert(strcmp(sig,
        "934250d3d1a1aae46bc5938467744bed4e13ac45a40d5af95b6ce76998abd279") == 0);
}

static void test_avatar_pattern(void)
{
    uint8_t a[PASSPORT_AVATAR_SIZE][PASSPORT_AVATAR_SIZE];
    uint8_t b[PASSPORT_AVATAR_SIZE][PASSPORT_AVATAR_SIZE];

    // 同输入同输出（稳定性）
    passport_avatar_pattern(TEST_PID, a);
    passport_avatar_pattern(TEST_PID, b);
    assert(memcmp(a, b, sizeof(a)) == 0);

    // 左右对称
    for (int r = 0; r < PASSPORT_AVATAR_SIZE; r++) {
        for (int c = 0; c < PASSPORT_AVATAR_SIZE / 2; c++) {
            assert(a[r][c] == a[r][PASSPORT_AVATAR_SIZE - 1 - c]);
            assert(a[r][c] <= 1);
        }
    }

    // 不同学号不同图案
    uint8_t other[PASSPORT_AVATAR_SIZE][PASSPORT_AVATAR_SIZE];
    passport_avatar_pattern("SM-2026-084", other);
    assert(memcmp(a, other, sizeof(a)) != 0);

    // 非全空非全满（避免退化成色块）
    int ones = 0;
    for (int r = 0; r < PASSPORT_AVATAR_SIZE; r++)
        for (int c = 0; c < PASSPORT_AVATAR_SIZE; c++) ones += a[r][c];
    assert(ones > 0 && ones < PASSPORT_AVATAR_SIZE * PASSPORT_AVATAR_SIZE);
}

int main(void)
{
    test_window();
    test_time_valid();
    test_qr_codes();
    test_qr_url();
    test_auth_sig();
    test_avatar_pattern();
    printf("test_passport_core: all assertions passed\n");
    return 0;
}
