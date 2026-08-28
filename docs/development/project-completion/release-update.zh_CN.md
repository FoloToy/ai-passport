<p align="right">
  <strong>简体中文</strong> · <a href="release-update.md">English</a>
</p>

# 动作 B：发布到 Git 并更新版本

本动作把固件或代码发布到版本仓库，并在需要时更新 GitHub/GitLab release。它是[项目开发完成流程](../project-completion.md)列出的六项可选动作之一。

这是提交到 Git 的路径，不是社区路径。先确认目的地；社区市场见 [publish-to-community.md](./publish-to-community.md)。

## 步骤

每一步都是外部、授权的变更。逐项与开发者分别确认——不要用一次笼统确认覆盖 commit、push、tag 与 release。

1. 提交改动并推送到 fork（`origin`）——分别确认。
2. 创建并推送 tag 以触发发布工作流——分别确认。
3. 让 tag 触发的构建产出合并固件 `.bin`。
4. 用产物创建或更新 GitHub/GitLab release——分别确认。工作流默认把发布标题设为版本号名；发布后把它完善为「项目功能名称 + 版本号」。
5. 用英文写发布说明（项目双语处配简体中文版），覆盖新增内容、如何构建、如何使用。
6. 在真机验证已发布的完整构建（见[发布后的真机验证](../project-completion.zh_CN.md#发布后的真机验证)）。

## 规则

- 遵循仓库提交与 PR 规则（[commit-and-pr.md](../../contribution/commit-and-pr.md)）。
- 遵循 fork 工作流（[fork-guide.md](../../fork-guide.md)）。
- tag 触发的构建运行 `build-firmware.yml`，它只在 tag 时发布。见 [CI-build-and-release.md](../CI-build-and-release.md)。
- 日常编译优先用 `idf.py build`（快、增量）；只有需要合并校验的 `0x0` 全镜像（如发布或交付前）才用 `./tools/validate.sh --firmware`。
- 工作流创建 release，默认标题为版本号名（来自 `softprops/action-gh-release` 的 `github.ref_name`）。发布后把标题完善为「项目功能名称 + 版本号」，例如 `Voice Keychain v1.2.0`。版本号为 tag，功能名称为共享[发布属性](../project-completion.md#共享发布属性)中的应用发布名。
- 发布说明必须向未读过仓库的用户解释构建：新增内容、如何构建、如何使用。

## 相关文档

- 打 tag 构建与发版：[CI-build-and-release.md](../CI-build-and-release.md)
- 提交与 PR 规则：[commit-and-pr.md](../../contribution/commit-and-pr.md)
- Fork 工作流：[fork-guide.md](../../fork-guide.md)
