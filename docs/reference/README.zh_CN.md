<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 参考（Reference）

本目录存放 AI Passport 开发中**不构成硬性要求**的参考资料：可复用的开发经验与已归档的应用档案。开发新东西时参考它们，而不是把它们当作强制规则。参考资料按贡献者的 GitHub 用户名组织：每个 `reference/<username>/` 目录下，经验条目以平铺文件存放，应用档案以子目录存放。

工程规则本身位于 [`../development/`](../development/README.zh_CN.md)；协作规范位于 [`../contribution/`](../contribution/README.zh_CN.md)。

## 贡献者

### Shinku-Chen

**经验条目：**

- [ESP32-C3 上音频压缩方式的权衡](shinku-chen/audio-compression-trade-offs.zh_CN.md) — 在有限 Flash 上如何为语音播放应用选编解码（IMA-ADPCM vs Opus vs MP3），含实测容量与解码器成本。
- [发布后收尾：AI Passport 发布流程的衔接](shinku-chen/post-release-follow-up.zh_CN.md) — 确认发布目的地、发布时包含数据分区、以及发布后收尾各轨道的同意门槛。
- [ESP32-C3（无 PSRAM）上的显示刷新与深睡](shinku-chen/display-refresh-and-deep-sleep.zh_CN.md) — 直接刷新单个图片矩形、RTC GPIO 深睡唤醒，以及 LVGL 对象类型误用的崩溃特征。

**应用档案：**

- [音效钥匙扣](shinku-chen/voice-keychain/README.zh_CN.md) — 把 AI Passport 变成口袋音频播放器的音效钥匙扣。
- [今天吃啥](shinku-chen/eat-what/README.zh_CN.md) — 按键驱动的食物轮盘，把 AI Passport 变成「今天吃什么」小转盘。

### PhoenixZHC

**经验条目：**

- [AI Passport 网络音频流与内存预算经验](phoenixzhc/network-audio-streaming-and-memory.zh_CN.md) — 有边界的 HTTP 音频流、ES8311/I2S 资源归属，以及解码、JSON、DMA 与 LVGL 的统一内存预算。
- [AI Passport SoftAP 配网与资源预算经验](phoenixzhc/softap-provisioning-and-resource-budget.zh_CN.md) — DHCP 状态、弹窗认证兼容、表单与上传边界，以及无 PSRAM 条件下的资源规划。

### Y2Lin

**经验条目：**

- [实现 FAP_SCREENSHOT_V1 串口截屏协议](y2lin/serial-screenshot-protocol.zh_CN.md) — 先装 USB-Serial-JTAG 驱动、按子串匹配命令、快照渲染进静态整屏缓冲、按发送环形缓冲分块流载荷、二进制窗口内静默日志。
- [音量计 UI：读数平滑、动画锚定与杂色块](y2lin/meter-ui-smoothing-and-layout.zh_CN.md) — 非对称 EMA 平滑实时读数、吉祥物动画锚定到创建位置、屏上杂色块的常见根因，以及 LVGL 池耗尽导致开机白屏。

## 新增经验条目

一次发布可沉淀**一条或多条**可复用经验，每条作为独立条目新增，以发布版本（tag 或 commit）作为上下文。遵守仓库语言规则：默认 `.md` 路径用英文、配套 `.zh_CN.md` 用简体中文，并在同一次变更中对齐。

条目是放在 `reference/<username>/` 下的单个 `.md`（及其 `.zh_CN.md`），按条目内容概要命名（小写连字符，例如 `audio-compression-trade-offs.md`），描述主题而非时间戳。每条经验在提交前分流：通用、上游也受益的经验作为 PR 提交到上游 `FoloToy/ai-passport`；纯 fork 定制按 [`docs/fork-guide.md`](../fork-guide.zh_CN.md) 留在 fork。

## 归档应用

应用发布后，在 `reference/<username>/<app-name>/` 下归档，附一份 AI 生成的双语功能说明（`README.md` / `.zh_CN.md`），可选配一份使用指南。档案为**纯文本**——封面图仅记录文件名与格式，不存放固件 `.bin`。`plays-archive` skill 驱动归档及其约定。

## 相关

- 仓库总览与 demo 分支：[`../README.md`](../README.zh_CN.md)
