// components/bsp/include/bsp_audio.h
// ES8311 音频 codec:I2C 走控制口(复用 bsp_i2c 的共享总线),I2S 走全双工数据口。
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>

// 初始化 codec 与 I2S。内部会调 bsp_i2c_init()(幂等),无需外部先调。成功调用可重复；
// 失败会释放本次已创建的 codec 接口和 I2S channel，修正故障后可重试。
esp_err_t bsp_audio_init(void);

// 设置采样格式。同格式重复调用是廉价的(直接复用已打开的 codec)。
//
// ⚠ 这里有个必须绕开的坑:esp_codec_dev_open() 在 codec【已打开】时会直接返回 OK 且
//   【不重新配置采样率】。若不先 close,16kHz 播完再播 8kHz 会以 16k 时钟送出 ——
//   音调和速度都快一倍。故本函数在格式变化时先 close 再 open。
// 控制口和数据口错误也会检查（不依赖 codec-dev 是否传播错误）；失败后停止
// I2S、强制休眠并释放 codec 对象，下次调用重建，不会复用半打开状态。
// 调用方须串行化格式/休眠/唤醒操作，且切换前停止所有 PCM 读写。
esp_err_t bsp_audio_set_format(uint32_t hz, uint8_t bits, uint8_t ch);

// 进入低功耗前强制执行 ES8311 完整 suspend 寄存器序列，回读关键寄存器
// （REG0E 只比较 bit6:0，其余寄存器比较全部位），
// 失败时重试一次，并显式停止 I2S TX/RX。该路径不依赖 codec-dev 是否打开过，
// 因此开机后从未播放也能正确暂停。调用前必须停止所有 PCM 读写；函数幂等，
// 音频未初始化时视为无需处理并返回成功。失败后的重复调用保留失败结果。
// 休眠时释放 codec 对象但保留共享 I2C 总线和 I2S channel；不依赖 codec
// 内部 enabled 标志恢复，唤醒会重新创建 codec 并恢复音量/格式。
esp_err_t bsp_audio_sleep(void);

// deep sleep 专用：确保 I2S TX/RX 已停止，再将 MCLK/BCLK/WS/DOUT/DIN
// 设为无上下拉的高阻输入。调用后 I2S 不能在本次运行中恢复，必须立即
// 进入 deep sleep 或重启；不得用于 light sleep。
esp_err_t bsp_audio_prepare_deep_sleep(void);

// 从 light sleep 返回后恢复 ES8311 和休眠前的采样格式。函数幂等；deep sleep
// 唤醒会重启应用，应由 bsp_audio_init() 按正常启动流程重新初始化。
esp_err_t bsp_audio_wake(void);

// 播放 / 录音。bytes 为字节数(16bit 单声道时 = 采样数 x 2)。
esp_err_t bsp_audio_write(const void *pcm, size_t bytes);
esp_err_t bsp_audio_read(void *pcm, size_t bytes);

// 输出音量 0..100(%)。
void bsp_audio_set_volume(uint8_t percent);

// 麦克风模拟输入增益（dB）。ES8311 的 PGA 范围约 0..30 dB。
//
// 默认 30 dB 对"人贴近卡片说话"是过载的 —— 实测 16bit 采样频繁打到 32768（削顶），
// 削平后的语音识别不出来。语音输入建议 12..18 dB；要现场调就用这个接口，
// 不必重新烧固件。音频未初始化时返回 ESP_ERR_INVALID_STATE。
esp_err_t bsp_audio_set_in_gain(float db);
