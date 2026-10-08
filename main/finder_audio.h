// main/finder_audio.h —— 盖革蜂鸣的输出 worker。
//
// 职责边界：本模块只负责"把 finder_beep 生成的样本按 DMA 节奏喂给 bsp_audio_write"。
// 节奏与波形全部由 finder_beep（纯逻辑、已宿主测试）决定。
//
// 任务优先级 5：高于 LVGL port 的 4（绘制不得把音频生产者饿到断流），
// 远低于 NimBLE host（configMAX_PRIORITIES-4）。用 finder_audio_set() 从任意上下文
// 改变等级，它只写一个共享字，不做重活。
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#define FINDER_AUDIO_CHUNK_SAMPLES 240 // 与 I2S DMA 描述符帧数一致

esp_err_t finder_audio_start(void);
esp_err_t finder_audio_stop(void);

// 任意上下文可调用。level 0 或 muted 为真时停止向 DMA 送数据（省掉编解码器与
// DMA 的持续功耗），恢复发声时从新的一次蜂鸣起跳。
void finder_audio_set(int8_t level, bool muted);

bool finder_audio_running(void);

// 最近一次播放失败原因；ESP_OK 表示正常。
esp_err_t finder_audio_last_error(void);
