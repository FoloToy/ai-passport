// main/finder_beep.h —— 盖革式蜂鸣的样本级流式生成器（纯逻辑，可宿主测试）。
//
// 为什么不是"每响一次调一次播放"：
//   bsp_audio_write 入队即返回，蜂鸣的起始时刻 = 当前队列深度 + 调用者时序抖动。
//   在 80ms 这种密集节奏下，队列深度不可控会导致蜂鸣成串、漂移。
// 所以这里改成【连续流式生成】：调用方按 DMA 块持续取样本，节奏由本模块内部
// 按采样点计数决定，与取块大小、与调用间隔无关。
//
// 音色：方波。方波本身在半个周期处翻转是"音调"不是咔哒；真正会产生咔哒的是
// 整段蜂鸣以满量程突然起停，因此这里对蜂鸣包络加了淡入淡出。
//
// 频率用相位累加器（Q32 定点）生成，不做周期整数除法——原 demo 的
// SAMPLE_RATE/TONE_HZ 整数除法会把 3000Hz 变成 3200Hz。
//
// 本文件不得包含 ESP-IDF / LVGL 头文件。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#define FINDER_BEEP_RATE_HZ   16000 // 与 ES8311 播放格式一致
#define FINDER_BEEP_PEAK       6000 // 峰值幅度，留足余量避免削顶
#define FINDER_BEEP_FADE_MS       3 // 包络淡入淡出时长
#define FINDER_BEEP_MAX_INTERVAL_MS 2500 // 等级 1 的间隔，决定周期计数上限

typedef struct {
    uint32_t sample_rate;
    int8_t   level;       // 当前等级 0..5
    bool     muted;
    uint32_t phase;       // 音调相位累加器（Q32）
    uint32_t tone_step;   // 每样本相位增量
    uint32_t cycle_pos;   // 当前节奏周期内已生成的样本数
    uint32_t cycle_len;   // 一个节奏周期的样本数
    uint32_t tone_len;    // 蜂鸣（发声）部分的样本数
    uint32_t fade_len;    // 淡入/淡出样本数
} finder_beep_gen_t;

// 等级 → 节奏间隔（毫秒）；等级 0 返回 0 表示静默。
uint32_t finder_beep_interval_ms(int8_t level);
// 等级 → 音调频率（Hz）；等级 0 返回 0。
uint32_t finder_beep_tone_hz(int8_t level);

void finder_beep_init(finder_beep_gen_t *gen, uint32_t sample_rate);

// 设置等级与静音。等级发生变化时节奏周期从头开始（靠得越近立刻就能听到），
// 静音切换不打断周期。
void finder_beep_set(finder_beep_gen_t *gen, int8_t level, bool muted);

// 生成 n 个 16-bit 单声道样本，返回实际生成数量。
// 输出与原调用顺序无关：分块大小不影响波形（这是本模块存在的理由）。
uint32_t finder_beep_next(finder_beep_gen_t *gen, int16_t *out, uint32_t n);
