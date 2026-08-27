<p align="right">
  <strong>简体中文</strong> · <a href="experience-notes.md">English</a>
</p>

# 开发经验沉淀

本页是每次固件发布后可复用开发经验的索引——聚焦 fork 相对上游 `docs/` 的差异：开发者在
fork 上自行创建或变更的 `docs/` 文档。每条经验一个 `.md` 及其配对 `.zh_CN.md`，保存在
[`../experiences/`](../experiences/) 目录下，按贡献者的 GitHub 用户名分组，按
条目内容概要命名（小写连字符）。`experience-pr` skill 生成新条目，并从下面索引链接它。

每条经验在提交前分流：通用、上游也受益的经验作为 PR 提交到上游 `FoloToy/ai-passport`；
纯 fork 定制按 [`docs/fork-guide.md`](../fork-guide.md) 留在 fork。

开始新开发前，可先查这里有没有之前沉淀、可复用的经验——与 [`plays/`](../../plays/README.md) 的
参考应用一起看。

## 如何新增一条

一次发布可沉淀**一条或多条**可复用经验，每条作为独立条目新增，以发布版本（tag 或 commit）作为
上下文。遵守仓库语言规则：默认 `.md` 路径用英文、配套 `.zh_CN.md` 用简体中文，并在同一次变更中
保持对齐。

条目存放在 `docs/experiences/<username>/` 下，按条目内容概要命名（小写连字符，例如
`audio-compression-trade-offs.md`），让文件名描述主题，而不是用不透明的时间戳。
`<username>` 是贡献开发者的 GitHub 用户名（英文小写连字符，如 `shinku-chen`），
把该开发者的条目聚在一起，而不是在 `docs/experiences/` 下平铺。

开发者不限于一条经验。档案保存**每位开发者一条或多条经验**，每条都是放在该开发者文件夹下的独立文件
（含其配对 `.zh_CN.md`），并各从下面的索引链接。每一条可复用的、发布后沉淀的经验都应**新增一条**，
而不是并入已有条目，以保证每条都是一个独立、自包含的主题。

## 条目

见 [`../experiences/`](../experiences/) 目录下已保存的条目，以及它的
[`INDEX.md`](../experiences/INDEX.md) 档案条目表。下面索引在条目新增后列出。

- **ESP32-C3 上音频压缩方式的权衡**（Shinku-Chen）— 在有限 Flash 上如何为语音播放应用选编解码（IMA-ADPCM vs Opus vs MP3），含实测容量与解码器成本。见 [`../experiences/shinku-chen/audio-compression-trade-offs.zh_CN.md`](../experiences/shinku-chen/audio-compression-trade-offs.zh_CN.md)。
- **发布后收尾：AI Passport 发布流程的衔接**（Shinku-Chen）— 确认发布目的地、发布时包含数据分区、以及发布后收尾各轨道的同意门槛。见 [`../experiences/shinku-chen/post-release-follow-up.zh_CN.md`](../experiences/shinku-chen/post-release-follow-up.zh_CN.md)。
- **ESP32-C3（无 PSRAM）上的显示刷新与深睡**（Shinku-Chen）— 直接刷新单个图片矩形、RTC GPIO 深睡唤醒，以及 LVGL 对象类型误用的崩溃特征。见 [`../experiences/shinku-chen/display-refresh-and-deep-sleep.zh_CN.md`](../experiences/shinku-chen/display-refresh-and-deep-sleep.zh_CN.md)。
