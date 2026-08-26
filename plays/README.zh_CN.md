<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# Plays（应用档案）

本目录位于上游 `FoloToy/ai-passport` 仓库，是 AI Passport 应用（plays）的档案库。它用于
**查询**每个应用做什么、怎么用，靠的是每个应用一份由 AI 生成的功能说明。它与社区发布流程关联：
发布固件后（[`docs/development/publish-to-community.md`](../docs/development/publish-to-community.md)），
助手会询问是否把这个应用归档到这里，并把它提案回上游。

## 开发新应用之前

开发新应用前，先查 `plays/` 里有没有已存在或可参考的项目，站在现有基础上做，而不是从零开始：

- 列出 `plays/` 下已归档的应用，读它们的功能说明，看是否已覆盖你的想法。
- 复用已有应用里合适的设计思路、交互模式或状态模型，而不是重新发明。
- 若没有合适的，记到将来该应用发布时再新建 `plays/<app-name>/` 档案。

每个 plays 子目录都是一个真实、可运行应用的档案；它的功能说明是你决定"扩展它还是参考它"
的起点。除应用档案外，也查一下
[`docs/development/experience-notes.md`](../docs/development/experience-notes.md)
里其他开发者已经沉淀、可复用的经验。

## 目录约定

每个应用一个子目录，目录名 = 应用名（小写连字符）。仅在应用发布或准备记录时建档，不要预先
创建空骨架。

```
plays/<app-name>/
  README.md / README.zh_CN.md         # AI 生成的双语功能说明
  <app-name>-cover.<webp|png|jpg>     # 封面图，commit（≤10 MiB）
```

## 每个应用 README 包含什么

每个应用目录下的 `README.md`（及其简体中文配对）是**为后续查询**而生成的 AI 功能说明，不是
发布产物。它记录：

- 应用名与一句话定位。
- 应用做什么、功能清单。
- 交互与玩法（按键、屏幕、流程）。
- 应用来源，用**仓库地址 + 分支 + 路径**三元组精确描述（例如 GitHub 仓库 URL、`feature/voice-keychain`
  分支、`main/voice_app.c`）。
- 封面图文件名与格式。

通过总结应用实现与行为来写，默认 `.md` 用英文、配对 `.zh_CN.md` 用简体中文，并在同一次变更
中对齐。

## 封面图

封面放在 `plays/<app-name>/<app-name>-cover.<webp|png|jpg>`，commit 进仓库（类似
`docs/assets/brand`）。选有代表性、且小于 10 MiB 的图。

## 固件

**不要**在这里保存合并固件二进制。`.bin` 是构建/发布流程产生的产物，不是仓库内资源。

## 相关

- 仓库总览与 demo 分支：[`../docs/README.md`](../docs/README.md)
- 软件设计索引：[`../docs/software-design/README.md`](../docs/software-design/README.md)
