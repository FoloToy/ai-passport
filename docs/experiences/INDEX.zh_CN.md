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
- [AI Passport 上的网络音频流式播放与内存预算](shinku-chen/network-audio-streaming.zh_CN.md) — 如何流式解码 MP3、驱动板载 ES8311，以及在 HTTP、DMA、任务栈与 LVGL 界面争抢无 PSRAM 内存时做内存预算。
- [AI Passport 上的 SoftAP 配网、DHCP 与认证弹窗](shinku-chen/softap-provisioning.zh_CN.md) — 如何搭 SoftAP + DHCP + HTTP 配网流程、让手机自动弹出认证页，以及避免表单缓冲溢出。
