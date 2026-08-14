#include <assert.h>
#include <string.h>
#include "stopwatch_core.h"

int main(void)
{
    char buf[16];

    stopwatch_format(0, buf, sizeof(buf));
    assert(strcmp(buf, "00:00.00") == 0);

    stopwatch_format(9999, buf, sizeof(buf));            // 0.009 秒,不足 1 百分位
    assert(strcmp(buf, "00:00.00") == 0);

    stopwatch_format(90000, buf, sizeof(buf));           // 0.09 秒
    assert(strcmp(buf, "00:00.09") == 0);

    stopwatch_format(10000, buf, sizeof(buf));           // 0.01 秒
    assert(strcmp(buf, "00:00.01") == 0);

    stopwatch_format(59999999, buf, sizeof(buf));        // 59.99 秒
    assert(strcmp(buf, "00:59.99") == 0);

    stopwatch_format(60000000, buf, sizeof(buf));        // 进位到 1 分钟
    assert(strcmp(buf, "01:00.00") == 0);

    stopwatch_format(3599999999ULL, buf, sizeof(buf));   // 59:59.99
    assert(strcmp(buf, "59:59.99") == 0);

    stopwatch_format(3600000000ULL, buf, sizeof(buf));   // 进位到 60 分钟
    assert(strcmp(buf, "60:00.00") == 0);

    return 0;
}
