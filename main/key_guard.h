// main/key_guard.h —— 深睡按键唤醒后的"按键动作守卫"状态机(纯逻辑,便于 host 测试)。
//
// 深睡按键唤醒会让芯片在"唤醒键还按着"的状态下重启。按键组件晚几百毫秒才启动,
// 它看到的是一个已经按下的电平,并会在按住时长达到门限(本板 500ms)时判成长按,
// 于是几乎立刻触发一个意料外的动作。守卫在唤醒键松手前吞掉所有按键动作。
//
// 关键点(也是原实现的 bug):守卫的撤防不能只靠"收到 release 事件"。若用户在按键
// 组件开始观测前就松了手,组件从头到尾没看到按下,永远不会发 release,守卫就会一直
// 拦到 deadline 兜底。所以按键驱动初始化完成后,必须按采样到的物理电平同步一次:
// 已松手立即撤防(见 key_guard_sync_released)。
#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    // armed 跨 main 任务与按键回调的 esp_timer 任务读写,标 volatile 以防被缓存。
    volatile bool armed;
    int64_t deadline_us;
} key_guard_t;

// 按键唤醒启动时武装。timeout_us 只作为按键卡住时的兜底。
static inline void key_guard_arm(key_guard_t *g, int64_t now_us, int64_t timeout_us) {
    g->armed = true;
    g->deadline_us = now_us + timeout_us;
}

// 每收到一个按键事件调一次。返回 true 表示该事件应被吞掉(不派发给应用),
// false 表示正常派发。release 事件或到期会就地撤防,但那次事件仍被吞掉。
static inline bool key_guard_consume(key_guard_t *g, bool is_release, int64_t now_us) {
    if (!g->armed) return false;
    if (is_release || now_us >= g->deadline_us) {
        g->armed = false;
    }
    return true;
}

// 按键驱动初始化完成后,按采样到的物理按键状态同步一次:
// released=true(已松手)立即撤防;released=false(仍按住)保持武装,直到收到
// release 事件或到期。采样失败时调用方应传 released=false,保守保持武装。
static inline void key_guard_sync_released(key_guard_t *g, bool released) {
    if (released) g->armed = false;
}
