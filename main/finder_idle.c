// main/finder_idle.c —— 息屏与降亮的闲置判定（纯逻辑，无 ESP-IDF / LVGL 依赖）。
#include "finder_idle.h"

// 无符号减法：32 位毫秒计数回绕（约 49 天）后仍然得到正确的间隔。
static uint32_t finder_idle_elapsed(uint32_t now_ms, uint32_t then_ms)
{
    return now_ms - then_ms;
}

void finder_idle_init(finder_idle_t *idle, uint32_t now_ms)
{
    idle->last_activity_ms = now_ms;
    idle->wake_ms = 0;
    idle->wake_valid = false;
}

bool finder_idle_note_key(finder_idle_t *idle, uint32_t now_ms)
{
    if (finder_idle_state_at(idle, now_ms) == FINDER_IDLE_OFF) {
        // 息屏下第一下：点亮。从这一刻起重新计时，否则刚点亮就可能立刻又息屏。
        idle->wake_ms = now_ms;
        idle->wake_valid = true;
        idle->last_activity_ms = now_ms;
        return true;
    }

    if (idle->wake_valid) {
        if (finder_idle_elapsed(now_ms, idle->wake_ms) < FINDER_IDLE_WAKE_GRACE_MS) {
            // 同一次按压的后续事件：屏幕已经亮了，但这一下不是用户的新意图。
            idle->last_activity_ms = now_ms;
            return true;
        }
        idle->wake_valid = false;
    }

    // 半亮或全亮下的按键照常生效：屏幕看得见，用户知道自己在按什么。
    idle->last_activity_ms = now_ms;
    return false;
}

finder_idle_state_t finder_idle_state_at(const finder_idle_t *idle, uint32_t now_ms)
{
    uint32_t idle_ms = finder_idle_elapsed(now_ms, idle->last_activity_ms);

    if (idle_ms >= FINDER_IDLE_OFF_MS) {
        return FINDER_IDLE_OFF;
    }
    if (idle_ms >= FINDER_IDLE_DIM_MS) {
        return FINDER_IDLE_DIM;
    }
    return FINDER_IDLE_AWAKE;
}

uint8_t finder_idle_backlight(finder_idle_state_t state)
{
    switch (state) {
    case FINDER_IDLE_DIM:
        return (uint8_t)FINDER_IDLE_DIM_PERCENT;
    case FINDER_IDLE_OFF:
        return 0;
    case FINDER_IDLE_AWAKE:
    default:
        return (uint8_t)FINDER_IDLE_FULL_PERCENT;
    }
}
