<p align="right">
  <strong>简体中文</strong> · <a href="CI-sync-main.md">English</a>
</p>

# 上游同步（CI / Upstream Sync）

本仓库提供一套基于 GitHub Actions 的自动上游同步流水线，用于定期把上游 `FoloToy/ai-passport` 的 `main` 分支更新同步到本 fork 的 `main` 分支。

本文件与 `.github/workflows/sync-main.yml` 一同维护，工作流行为变化时必须同步更新。

## 触发条件

- **schedule**：每天 00:00（UTC）自动运行一次。
- **workflow_dispatch**：可在 GitHub Actions 页面手动触发（用于立即同步/排查问题）。

> 该工作流仅在仓库为 **fork** 时生效（`if: github.event.repository.fork`）；非 fork 仓库不运行。

## 流水线做了什么

1. **Checkout 目标仓库**：`actions/checkout` 检出当前 fork 的 `main` 分支，并关闭 Git 凭证持久化。
2. **同步上游**：使用固定到完整 commit SHA 的 `aormsby/fork-sync-with-upstream-action`（对应 v3.4.3），把 `FoloToy/ai-passport` 的 `main` 同步到本 fork 的 `main`。`target_repo_token` 使用自动生成且仅具 `contents: write` 权限的 `GITHUB_TOKEN`，无需手动配置。
3. **失败检查**：同步失败时输出提示——上游 workflow 文件变更可能导致 GitHub 暂停自动同步，需手动 Sync Fork 一次。

## 注意事项

- 同步目标与上游分支均为 `main`，与 fork 用户约定（`main` 仅允许修改根目录 `README.md` 与 `docs/assets/`）配合使用：`main` 保持与上游最新基线同步、不产生冲突。
- **在 `main` 直接开发的例外**：如果用户执意要在 `main` 分支直接开发，必须**停用/关闭本 workflow**（Actions 页面 → Disable），否则每日自动同步会把上游改动强行合入 `main`，产生冲突或覆盖本地开发内容。
- 若同步失败，查看 Actions 日志确认是否为上游 workflow 文件变更所致；必要时按提示手动在 GitHub 页面 Sync Fork。
- 升级 Action 时必须从官方仓库核对目标版本对应的完整 commit SHA，并同步更新 workflow 行尾的版本注释。

## 相关文件

- `.github/workflows/sync-main.yml`：本流水线定义。
