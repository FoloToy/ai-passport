// main/stopwatch_core.c —— 秒表时间格式化,纯函数,无硬件依赖。
#include "stopwatch_core.h"
#include <stdio.h>

void stopwatch_format(uint64_t us, char *buf, size_t n)
{
    uint64_t cs = us / 10000;              // 百分之一秒
    uint64_t m  = cs / 6000;
    uint64_t s  = (cs / 100) % 60;
    uint64_t c  = cs % 100;
    snprintf(buf, n, "%02llu:%02llu.%02llu",
             (unsigned long long)m, (unsigned long long)s, (unsigned long long)c);
}
