// 配网表单解析的 host 测试:不依赖 SDK,只验证 prov_form 的纯字符串逻辑。
// 重点覆盖 URL 解码边界(表单里最容易出错的地方)和长度校验。
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "prov_form.h"

static void test_parse_basic(void) {
    char ssid[40];
    char pass[72];
    const char body[] = "ssid=MyHome-2.4G&pass=passw0rd!42";
    assert(prov_form_parse(body, strlen(body), ssid, sizeof(ssid), pass, sizeof(pass)));
    assert(strcmp(ssid, "MyHome-2.4G") == 0);
    assert(strcmp(pass, "passw0rd!42") == 0);
}

static void test_parse_order_and_extra(void) {
    char ssid[40];
    char pass[72];
    // 字段顺序无关,未知字段忽略,密码可缺省(开放网络)。
    const char body[] = "foo=1&pass=&ssid=Home-5G&bar=2";
    assert(prov_form_parse(body, strlen(body), ssid, sizeof(ssid), pass, sizeof(pass)));
    assert(strcmp(ssid, "Home-5G") == 0);
    assert(pass[0] == '\0');
}

static void test_parse_url_decoding(void) {
    char ssid[40];
    char pass[72];
    // %E5%AE%B6 = "家";'+' 还原成空格;%2B 是真正的加号。
    const char body[] = "ssid=%E5%AE%B6+wifi&pass=a%2Bb%20c";
    assert(prov_form_parse(body, strlen(body), ssid, sizeof(ssid), pass, sizeof(pass)));
    assert(strcmp(ssid, "家 wifi") == 0);
    assert(strcmp(pass, "a+b c") == 0);
}

static void test_parse_rejects_bad_input(void) {
    char ssid[8];
    char pass[16];
    // 没有 ssid 字段。
    const char no_ssid[] = "pass=12345678";
    assert(!prov_form_parse(no_ssid, strlen(no_ssid), ssid, sizeof(ssid), pass, sizeof(pass)));
    // 转义不完整。
    const char bad_esc[] = "ssid=abc%2";
    assert(!prov_form_parse(bad_esc, strlen(bad_esc), ssid, sizeof(ssid), pass, sizeof(pass)));
    // 非法十六进制。
    const char bad_hex[] = "ssid=abc%ZZ";
    assert(!prov_form_parse(bad_hex, strlen(bad_hex), ssid, sizeof(ssid), pass, sizeof(pass)));
    // 解码后超长:宁可不截断,直接失败。
    const char too_long[] = "ssid=abcdefghijkl";
    assert(!prov_form_parse(too_long, strlen(too_long), ssid, sizeof(ssid), pass, sizeof(pass)));
}

static void test_check(void) {
    char long_ssid[40];
    memset(long_ssid, 'a', sizeof(long_ssid));
    long_ssid[33] = '\0';

    char long_pass[80];
    memset(long_pass, 'p', sizeof(long_pass));
    long_pass[64] = '\0';

    assert(prov_form_check("MyHome-2.4G", "passw0rd!42") == PROV_FORM_OK);
    assert(prov_form_check("MyHome-2.4G", "") == PROV_FORM_OK);          // 开放网络
    assert(prov_form_check("", "12345678") == PROV_FORM_ERR_SSID_EMPTY);
    assert(prov_form_check(long_ssid, "") == PROV_FORM_ERR_SSID_LONG);
    assert(prov_form_check("MyHome-2.4G", "1234567") == PROV_FORM_ERR_PASS_SHORT);
    assert(prov_form_check("MyHome-2.4G", long_pass) == PROV_FORM_ERR_PASS_LONG);
    // 边界值应当通过:32 字节 SSID、63 位密码。
    assert(prov_form_check("01234567890123456789012345678901", long_pass - 1) == PROV_FORM_OK);
    assert(strlen(prov_form_error_text(PROV_FORM_ERR_PASS_SHORT)) > 0);
    assert(strlen(prov_form_error_text(PROV_FORM_OK)) == 0);
}

int main(void) {
    test_parse_basic();
    test_parse_order_and_extra();
    test_parse_url_decoding();
    test_parse_rejects_bad_input();
    test_check();
    printf("test_prov_form: OK\n");
    return 0;
}
