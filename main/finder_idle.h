// main/finder_idle.h —— 息屏与降亮的闲置判定（纯逻辑，可宿主测试）。
//
// 目的：Pocket Finder 会让用户拿着设备走动、听蜂鸣逼近目标，期间可能几分钟不按键。
// 屏幕一直全亮是纯浪费，所以按"最后一次按键"计时分两档：
//   闲置 1 分钟 → 背光减半（还看得见，只是省电）
//   闲置 3 分钟 → 息屏（背光 0；面板仍显示最后一帧，只是不发光）
//
// ★ 息屏后的第一次按键只用于点亮，不投给玩法状态机。理由：本应用的"短按 OK = 下一个设备"
//   本身就是静默传送（PRD 风险 R10），在看不见屏幕的情况下再让它触发动作，
//   用户根本无法知道自己刚才改了什么。一次真实按压会依次产生 PRESS → RELEASE → CLICK
//   （以及长按时的 LONG），因此这些后续事件必须在唤醒宽限期内一并吞掉，
//   只吞 PRESS 是不够的。
//
// ★ 计时刻意用 32 位毫秒（xTaskGetTickCount() 的原生宽度）而不是 int64：
//   本结构被【按键任务】写、被【LVGL 任务】读，32 位对齐读写在 ESP32-C3 上是单条指令，
//   不可能读到撕裂值；宽度必须配无符号回绕减法使用（见 finder_idle_elapsed）。
//
// 本文件不得包含 ESP-IDF / LVGL 头文件：它必须能在宿主机上用 cc 直接编译。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FINDER_IDLE_DIM_MS         60000u  // 闲置到此值 → 背光减半
#define FINDER_IDLE_OFF_MS        180000u  // 闲置到此值 → 息屏
#define FINDER_IDLE_DIM_PERCENT       50u  // "亮度减半"：全亮 100 的一半
#define FINDER_IDLE_FULL_PERCENT     100u
// 唤醒那一次按压的后续事件（RELEASE / CLICK / LONG）都在这个窗口内到达，一并吞掉。
// 取 600ms 是为了覆盖 500ms 长按判定：按住不放的用户只应点亮屏幕，不应触发长按动作。
#define FINDER_IDLE_WAKE_GRACE_MS    600u

typedef enum {
    FINDER_IDLE_AWAKE = 0,  // 全亮
    FINDER_IDLE_DIM,        // 背光减半
    FINDER_IDLE_OFF,        // 息屏
} finder_idle_state_t;

typedef struct {
    uint32_t last_activity_ms;  // 最后一次按键的时刻（唯一写入者：按键任务）
    uint32_t wake_ms;           // 最近一次从息屏唤醒的时刻
    bool     wake_valid;        // wake_ms 是否有效
} finder_idle_t;

// 初始化：从"刚刚有活动"开始计时，因此进入应用后不会立刻降亮。
void finder_idle_init(finder_idle_t *idle, uint32_t now_ms);

// 记录一次按键输入，返回 true 表示【这次输入必须被丢弃】（它只用于点亮屏幕）。
// 由按键任务调用；同时它也是闲置计时的唯一写入者。
bool finder_idle_note_key(finder_idle_t *idle, uint32_t now_ms);

// 纯查询：now_ms 时刻应处于哪一档。不修改状态，可由另一任务安全读取。
finder_idle_state_t finder_idle_state_at(const finder_idle_t *idle, uint32_t now_ms);

// 各档对应的背光百分比（0..100）。
uint8_t finder_idle_backlight(finder_idle_state_t state);
