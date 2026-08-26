<p align="right">
  <strong>简体中文</strong> · <a href="publish-to-community.md">English</a>
</p>

# 发布到 AI Passport 社区

本文说明如何把本项目的固件发布到 [AI Passport 社区](https://ai-passport.folotoy.cn)。本页是面向人的入口；它告诉 AI 助手从官方包安装发布 skill，并运行该工作流。

## 何时使用

当你希望把当前固件（或某个 `demo/*` 应用）发布到社区市场时使用。它不用于普通固件开发或烧录。

该工作流由官方发布 skill 驱动。运行一次提示词，让助手从官方包安装 skill；仓库无需提交任何东西。

## 使用内置提示词

要运行发布工作流，把这段提示词交给你的 AI 助手：

```text
请安装 FoloToy AI Passport 发布助手：https://ai-passport.folotoy.cn/skills/folotoy-ai-passport-publisher.zip
然后分析当前项目并准备发布到 AI Passport 社区。请检查完整固件，从 README、文档和代码中整理中英文标题与简介，准备项目封面，并使用当前 GitHub、Gitee 或其他 HTTPS Git 仓库地址。如果尚未登录，请引导我在官网注册或登录并完成授权；正式上传前，先把全部内容展示给我确认。
```

skill 的 `SKILL.md` 定义了精确流程：检查项目、准备中英文标题与简介、解析 HTTPS Git 源码、准备并校验封面、经官方站点授权，然后在真正上传前展示每个字段并取得批准。

## 你需要提供给助手的东西

- **固件**：单个合并的 ESP `.bin`（从 `0x0` 烧录）。必要时用 `./tools/validate.sh --firmware` 或 `idf.py build` 构建。
- **封面**：一张代表产品的 JPEG / PNG / WebP 图（≤ 10 MiB）。
- **源码**：固件仓库的公开 HTTPS Git 项目页——GitHub、Gitee、GitLab、Codeberg 或其它公开可达的 HTTPS Git 仓库页。fork 所有者从其 fork 的来源页发布，从 `git remote -v` 解析。

## 安全与边界

- 只上传到 `https://ai-passport.folotoy.cn`。发布与更新是外部变更。
- 未经作者确认的验证、起草与预览**不授权上传**。
- 助手绝不索取、接收或存储授权凭证。由创作者在官方站点注册或登录并批准显示的代码；助手不接触其密码。
- 不自动重试被拒的上传。先把服务端响应展示给创作者，查清原因再处理。

## 助手如何安装该 skill

助手从提示词里的 URL 拉取官方包，并按其中 `SKILL.md` 描述的工作流执行。本仓库无需保留或提交该 skill；提示词每次都会复现官方安装源。

## 发布之后：归档到 plays

固件发布后，询问开发者是否把该应用归档到仓库的 [`plays/`](../../plays/README.md)
应用档案。若同意，在 `plays/<app-name>/` 下生成该应用的 AI 功能总结（双语
`README.md` / `.zh_CN.md`），并添加封面图 `plays/<app-name>/<app-name>-cover.<webp|png|jpg>`。
只提交总结与封面；**不要**在这里存固件 `.bin`。

这样发布后的应用能在仓库内留存、便于后续查询。
