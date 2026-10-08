// main/finder_beep.c —— 见 finder_beep.h。纯 C，无 ESP-IDF 依赖。
#include "finder_beep.h"

// 节奏表（PRD §11.1）：等级越高，间隔越短。
static const uint32_t k_interval_ms[6] = { 0, 2500, 1000, 500, 200, 80 };
// 音调：1..4 用 2000Hz；等级 5 再提高一档（PRD 规定 3000Hz）。
// 基准 2000Hz 是标定值，可调。
static const uint32_t k_tone_hz[6] = { 0, 2000, 2000, 2000, 2000, 3000 };

#define FINDER_BEEP_TONE_MS 40

static uint32_t ms_to_samples(uint32_t rate, uint32_t ms)
{
    return (uint32_t)(((uint64_t)rate * ms) / 1000u);
}

uint32_t finder_beep_interval_ms(int8_t level)
{
    if (level < 0 || level > 5) {
        return 0;
    }
    return k_interval_ms[level];
}

uint32_t finder_beep_tone_hz(int8_t level)
{
    if (level < 0 || level > 5) {
        return 0;
    }
    return k_tone_hz[level];
}

static int8_t clamp_level(int8_t level)
{
    if (level < 0) {
        return 0;
    }
    if (level > 5) {
        return 5;
    }
    return level;
}

static void apply_level(finder_beep_gen_t *gen, int8_t level)
{
    gen->level = clamp_level(level);
    // 周期从头开始：用户靠得更近时应该立刻听到回应，而不是等上一轮走完。
    gen->cycle_pos = 0;
    gen->phase = 0;

    uint32_t tone_hz = finder_beep_tone_hz(gen->level);
    gen->cycle_len = ms_to_samples(gen->sample_rate, finder_beep_interval_ms(gen->level));
    gen->tone_len = (tone_hz == 0) ? 0 : ms_to_samples(gen->sample_rate, FINDER_BEEP_TONE_MS);
    if (gen->tone_len > gen->cycle_len) {
        gen->tone_len = gen->cycle_len;
    }
    // Q32 相位增量：hz/rate * 2^32。先左移再除，避免整数周期误差
    // （原 demo 的 rate/hz 整数除法会把 3000Hz 变成 3200Hz）。
    gen->tone_step = (tone_hz == 0) ? 0 : (uint32_t)(((uint64_t)tone_hz << 32) / gen->sample_rate);
}

void finder_beep_init(finder_beep_gen_t *gen, uint32_t sample_rate)
{
    if (!gen || sample_rate == 0) {
        return;
    }
    gen->sample_rate = sample_rate;
    gen->muted = false;
    gen->fade_len = ms_to_samples(sample_rate, FINDER_BEEP_FADE_MS);
    apply_level(gen, 0);
}

void finder_beep_set(finder_beep_gen_t *gen, int8_t level, bool muted)
{
    if (!gen) {
        return;
    }
    int8_t clamped = clamp_level(level);
    if (clamped != gen->level) {
        apply_level(gen, clamped);
    }
    // 静音只是输出闸门，不打断节奏时钟（见 finder_beep_next 的注释）。
    gen->muted = muted;
}

// 包络：淡入淡出抑制整段蜂鸣起停时的咔哒。方波在半周期处的翻转是音调本身，
// 不需要也不应该去"修"。
static int32_t envelope(int32_t amp, uint32_t pos, const finder_beep_gen_t *gen)
{
    if (gen->fade_len == 0) {
        return amp;
    }
    if (pos < gen->fade_len) {
        return (int32_t)((int64_t)amp * (int32_t)pos / (int32_t)gen->fade_len);
    }
    if (pos + gen->fade_len >= gen->tone_len) {
        uint32_t remaining = gen->tone_len - pos;
        return (int32_t)((int64_t)amp * (int32_t)remaining / (int32_t)gen->fade_len);
    }
    return amp;
}

uint32_t finder_beep_next(finder_beep_gen_t *gen, int16_t *out, uint32_t n)
{
    if (!gen || !out) {
        return 0;
    }

    bool audible = !gen->muted && gen->level > 0 && gen->cycle_len > 0;

    for (uint32_t i = 0; i < n; i++) {
        int16_t sample = 0;
        if (audible && gen->cycle_pos < gen->tone_len) {
            int32_t amp = (gen->phase & 0x80000000u) ? FINDER_BEEP_PEAK : -FINDER_BEEP_PEAK;
            sample = (int16_t)envelope(amp, gen->cycle_pos, gen);
        }
        out[i] = sample;

        // 时钟始终推进——即使静音。这样"取消静音"能立刻听到当下状态，
        // 而不是从一个被冻结的位置重新开始；也让输出只取决于样本计数，
        // 与调用方怎么分块无关（这是本模块相对"每响一声写一次"的核心优势）。
        if (gen->cycle_len > 0) {
            gen->phase += gen->tone_step;
            gen->cycle_pos++;
            if (gen->cycle_pos >= gen->cycle_len) {
                gen->cycle_pos = 0;
                gen->phase = 0; // 每次蜂鸣从同相位起跳，听感一致
            }
        }
    }
    return n;
}
