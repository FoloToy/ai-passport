<p align="right">
  <strong>简体中文</strong> · <a href="publish-to-community.md">English</a>
</p>

# 发布到 AI Passport 社区

本文说明如何把本项目的固件发布到 [AI Passport 社区](https://ai-passport.folotoy.cn)。本页是面向人的入口；它告诉 AI 助手从官方包安装发布 skill，并运行该工作流。

## 何时使用

当项目进入**发布**时使用——也就是**代码开发完毕、能够使用、可以交付**的时候。发布不是提交：提交是开发过程中保存进度，而发布只在开发完成、固件真正可用时才发生。本页讲发布决策；开发本身遵循开发与提交规则。

当开发者说"发布"或"发版"时，第一件事是决定**发到哪里**。不要默认其中一条。

- **发布到社区** —— 把固件发布到 [AI Passport 社区市场](https://ai-passport.folotoy.cn)。走下面的 publisher 工作流；发布后见项目开发完成流程（`docs/development/project-completion.md`）。
- **发布到 Git** —— 把开发完成的代码发布到版本仓库。逻辑：先**提交**完成后的代码，再为它**开 release 流程**。release 产物可由 **CI/CD 流水线生成（推荐）**，tag 触发 `.github/workflows/build-firmware.yml` 自动构建；也**可用开发者本地已构建的 `.bin` 文件**。遵循仓库提交与 PR 规则（`docs/contribution/commit-and-pr.md`）与 fork 工作流（`docs/fork-guide.md`）；自动构建与产物见 [`CI-build-and-release.md`](CI-build-and-release.md)。
- **还没到发布时机** —— 代码尚未开发完成、不能使用；继续开发，不发布。

该工作流由官方发布 skill 驱动。运行一次提示词，让助手从官方包安装 skill；仓库无需提交任何东西。

## 使用内置提示词

要运行发布工作流，把这段提示词交给你的 AI 助手：

```text
请安装 FoloToy AI Passport 发布助手：https://ai-passport.folotoy.cn/skills/folotoy-ai-passport-publisher.zip
然后分析当前项目并准备发布到 AI Passport 社区。请检查完整固件，从 README、文档和代码中整理中英文标题与简介，准备项目封面，并使用当前 GitHub、Gitee 或其他 HTTPS Git 仓库地址。如果尚未登录，请引导我在官网注册或登录并完成授权；正式上传前，先把全部内容展示给我确认。
```

skill 的 `SKILL.md` 定义了精确流程：检查项目、准备中英文标题与简介、解析 HTTPS Git 源码、准备并校验封面、经官方站点授权，然后在真正上传前展示每个字段并取得批准。

## 你需要提供给助手的东西

- **固件**：固定使用单个合并镜像 `build/FoloToy-AI-Passport-full.bin`，
  且必须通过 `./tools/validate.sh --firmware` 与[小程序 BLE 兼容契约](ble-recovery-compatibility.zh_CN.md)。
  不得用 `idf.py build` 生成的应用单镜像替代。
- **封面**：一张代表产品的 JPEG / PNG / WebP 图（≤ 10 MiB）。
- **源码**：固件仓库的公开 HTTPS Git 项目页——GitHub、Gitee、GitLab、Codeberg 或其它公开可达的 HTTPS Git 仓库页。fork 所有者从其 fork 的来源页发布，从 `git remote -v` 解析。

## 安全与边界

- 只上传到 `https://ai-passport.folotoy.cn`。发布与更新是外部变更。
- 未经作者确认的验证、起草与预览**不授权上传**。
- 助手绝不索取、接收或存储授权凭证。由创作者在官方站点注册或登录并批准显示的代码；助手不接触其密码。
- 不自动重试被拒的上传。先把服务端响应展示给创作者，查清原因再处理。
- 不得为了让社区提交通过而弱化、绕过或删除 BLE 兼容门禁；应修复镜像布局或打包流程。

## 助手如何安装该 skill

助手从提示词里的 URL 拉取官方包，并按其中 `SKILL.md` 描述的工作流执行。本仓库无需保留或提交该 skill；提示词每次都会复现官方安装源。

## 发布之后：归档到 plays

固件发布后，询问开发者是否把该应用归档到上游仓库的 [`plays/`](../../plays/README.md)
应用档案。若同意，在 `plays/<username>/<app-name>/` 下生成该应用的 AI 功能总结（双语
`README.md` / `.zh_CN.md`）。档案为**纯文本**：封面图只记录文件名与格式作为发布元数据，
**不要**把封面图提交进 `plays/`（约定见 [`plays/README.md`](../../plays/README.md)）。
也不要在这里存固件 `.bin`。用 `plays-archive` skill，它会向上游
`FoloToy/ai-passport` 开归档 PR。

这样发布后的应用能在上游仓库留存、便于后续查询。
