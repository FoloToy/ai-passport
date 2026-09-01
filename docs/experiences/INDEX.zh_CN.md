<p align="right">
  <strong>简体中文</strong> · <a href="INDEX.md">English</a>
</p>

# 开发经验档案索引

本页列出 [`docs/experiences/`](../development/experience-notes.md) 下所有已记录的开发经验条目，
按贡献开发者的 GitHub 用户名分组。每条是发布后可复用的经验，由 `experience-pr` skill 写入并索引。

如何新增条目、哪些内容归属这里，见[经验索引](../development/experience-notes.md)。

## 索引

每条经验保存在 `docs/experiences/<username>/` 下，并在下面按贡献开发者的 GitHub 用户名分组列出。
一位开发者可有**一条或多条**经验；每条都是独立记录，新经验**新增一条**，而不是并入已有条目。

### Shinku-Chen

- [ESP32-C3 上音频压缩方式的权衡](shinku-chen/audio-compression-trade-offs.zh_CN.md) — 在有限 Flash 上如何为语音播放应用选编解码（IMA-ADPCM vs Opus vs MP3），含实测容量与解码器成本。
- [发布后收尾：AI Passport 发布流程的衔接](shinku-chen/post-release-follow-up.zh_CN.md) — 确认发布目的地、发布时包含数据分区、以及发布后收尾各轨道的同意门槛。
- [ESP32-C3（无 PSRAM）上的显示刷新与深睡](shinku-chen/display-refresh-and-deep-sleep.zh_CN.md) — 直接刷新单个图片矩形、RTC GPIO 深睡唤醒，以及 LVGL 对象类型误用的崩溃特征。

### PhoenixZHC

- [AI Passport 网络音频流与内存预算经验](phoenixzhc/network-audio-streaming-and-memory.zh_CN.md) — 有边界的 HTTP 音频流、ES8311/I2S 资源归属，以及解码、JSON、DMA 与 LVGL 的统一内存预算。
- [AI Passport SoftAP 配网与资源预算经验](phoenixzhc/softap-provisioning-and-resource-budget.zh_CN.md) — DHCP 状态、弹窗认证兼容、表单与上传边界，以及无 PSRAM 条件下的资源规划。

### Y2Lin

- [实现 FAP_SCREENSHOT_V1 串口截屏协议](y2lin/serial-screenshot-protocol.zh_CN.md) — 先装 USB-Serial-JTAG 驱动、按子串匹配命令、快照渲染进静态整屏缓冲、按发送环形缓冲分块流载荷、二进制窗口内静默日志。
- [音量计 UI：读数平滑、动画锚定与杂色块](y2lin/meter-ui-smoothing-and-layout.zh_CN.md) — 非对称 EMA 平滑实时读数、吉祥物动画锚定到创建位置、屏上杂色块的常见根因，以及 LVGL 池耗尽导致开机白屏。
