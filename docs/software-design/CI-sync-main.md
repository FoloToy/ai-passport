# 上游同步（CI / Upstream Sync）

本仓库提供一套基于 GitHub Actions 的自动上游同步流水线，用于定期把上游 `FoloToy/ai-passport` 的 `main` 分支更新同步到本 fork 的 `main` 分支。

> 本文件随 CI 工作流维护在 `ci/sync-main` 分支，与上游 main 的软件设计文档分开管理。

## 触发条件

- **schedule**：每天 00:00（UTC）自动运行一次。
- **workflow_dispatch**：可在 GitHub Actions 页面手动触发（用于立即同步/排查问题）。

> 该工作流仅在仓库为 **fork** 时生效（`if: github.event.repository.fork`）；非 fork 仓库不运行。

## 流水线做了什么

1. **Checkout 目标仓库**：`actions/checkout` 检出当前 fork 的 `main` 分支。
2. **同步上游**：使用 `aormsby/fork-sync-with-upstream-action@v3.4.3`，把 `FoloToy/ai-passport` 的 `main` 同步到本 fork 的 `main`。`target_repo_token` 使用自动生成的 `GITHUB_TOKEN`，无需手动配置。
3. **失败检查**：同步失败时输出提示——上游 workflow 文件变更可能导致 GitHub 暂停自动同步，需手动 Sync Fork 一次。

## 注意事项

- 同步目标与上游分支均为 `main`，与 fork 用户约定（`main` 仅允许修改 `README.md` 与 `assets/docs/`）配合使用：`main` 保持与上游最新基线同步、不产生冲突。
- 若同步失败，查看 Actions 日志确认是否为上游 workflow 文件变更所致；必要时按提示手动在 GitHub 页面 Sync Fork。

## 相关文件

- `.github/workflows/sync-main.yml`：本流水线定义。
