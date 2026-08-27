<p align="right">
  <strong>简体中文</strong> · <a href="experience-notes.md">English</a>
</p>

# 开发经验沉淀

本页是每次固件发布后可复用开发经验的索引——聚焦 fork 相对上游 `docs/` 的差异：开发者在
fork 上自行创建或变更的 `docs/` 文档。每次采集生成一个 `.md` 及其配对 `.zh_CN.md`，保存在
[`../experiences/`](../experiences/) 目录下，按贡献者的 GitHub 用户名分组，命名为
`<unixtime>_<commit-sha>.md`。`experience-pr` skill 生成新条目，并从下面索引链接它。

每条经验在提交前分流：通用、上游也受益的经验作为 PR 提交到上游 `FoloToy/ai-passport`；
纯 fork 定制按 [`docs/fork-guide.md`](../fork-guide.md) 留在 fork。

开始新开发前，可先查这里有没有之前沉淀、可复用的经验——与 [`plays/`](../../plays/README.md) 的
参考应用一起看。

## 如何新增一条

每次发布追加一条带日期的记录，以发布版本（tag 或 commit）作为上下文。遵守仓库语言规则：
默认 `.md` 路径用英文、配套 `.zh_CN.md` 用简体中文，并在同一次变更中保持对齐。

条目存放在 `docs/experiences/<username>/` 下，命名为本次采集的 Unix 时间戳加相关 commit 短 SHA。
`<username>` 是贡献开发者的 GitHub 用户名（小写连字符），把该开发者的条目聚在一起，而不是在
`docs/experiences/` 下平铺。

## 条目

见 [`../experiences/`](../experiences/) 目录下已保存的条目，以及它的
[`INDEX.md`](../experiences/INDEX.md) 档案条目表。下面索引在条目新增后列出。

- **ESP32-C3 上音频压缩方式的权衡**（Shinku-Chen）— 在有限 Flash 上如何为语音播放应用选编解码（IMA-ADPCM vs Opus vs MP3），含实测容量与解码器成本。见 [`../experiences/Shinku-Chen/1787793847_91466b0.zh_CN.md`](../experiences/Shinku-Chen/1787793847_91466b0.zh_CN.md)。
