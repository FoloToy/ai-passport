// main/stopwatch_core.h —— 秒表纯逻辑(与 UI/硬件无关,可主机单测)。
#pragma once

#include <stddef.h>
#include <stdint.h>

// 把微秒时长格式化为 "MM:SS.CC"(分:秒.百分之一秒),写入 buf。
void stopwatch_format(uint64_t us, char *buf, size_t n);
