<p align="right">
  <strong>简体中文</strong> · <a href="after-release.md">English</a>
</p>

# 发布后收尾

一旦 **release 全部完成**——无论发布到 AI Passport 社区还是发布到 Git——就把发布闭环收尾，按本页执行后续工作。发布本身见
[publish-to-community.md](publish-to-community.md)；本页是发布**之后**做什么的总览。

收尾沿四条独立轨道进行。轨道 1 是设备检查；其余由仓库内的 skill 驱动并遵循相同的安全与同意门槛。
都不负责发布固件。

## 轨道 1：重刷发布产物并验证

release 完成后，下载该 release 的**合并完整固件**（`FoloToy-AI-Passport-full.bin`，可从 `0x0` 烧录的完整版），
刷入设备并确认能正常运行。不要把"构建成功"或"上传成功"当成硬件验证：这一步证明 release 实际指向的产物
能在真机上启动并正常工作。产物来自 release 资产（CI/CD 的 `full.bin`），若 Git release 无 CI 产物则用
开发者本地构建的 `full.bin`。若不能运行，先停下来修复再做后面轨道。产物与烧录见
[`CI-build-and-release.md`](CI-build-and-release.md)。

## 轨道 2：归档到 plays（可选） + 整理 README（必须）

发布后，**提议**把该应用归档到上游 `FoloToy/ai-passport` 仓库的 `plays/` 应用档案。归档是**可选的**——
开发者可以拒绝归档，这个选择被尊重，且不影响后续收尾。若同意，在 `plays/<username>/<app-name>/` 下生成
该应用的 AI 功能总结（双语 `README.md` / `.zh_CN.md`，仅文字，不 commit 封面图），只提交总结文字（及
可选手册）；**不要**在这里存固件 `.bin`。见 [`../../plays/README.md`](../../plays/README.md)。

**无论是否归档，README 整理都必须进行。** 即使跳过归档到 `plays/`，仍要更新**承载分支**上的 README 与
fork `main` 的 README，让应用登记到它开发的地方：

- 在**承载分支**上：没有根 README 就创建双语配对，有则主动更新（加该应用自身说明）。
- 在 fork **main** 上：更新根 README，让它**完整囊括**每个项目自己 README 的内容——完整说明该应用
  做什么、怎么用——而不是一行简介 + 分支链接；内容取自承载分支的 README。

**建议：这些 README 更新直接提交/合并，而不是开 PR。** fork 根 README 与承载分支的根 README 是 fork
自有内容（不属于上游提案），直接 commit 到分支 / fork `main` 即可，不必等 review PR。只有当改动是要回到
上游时才开 PR。

## 轨道 3：收集建议并提交 issue

运行 `issue-suggestions` skill，收集发布固件的开发者本人的改进点，把有价值的整理成功能
建议 issue，提交到上游 `FoloToy/ai-passport` 项目。

1. 确认开发者同意开始收尾（涉及项目私有内容）。
2. 确认已有可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）；否则把草案交给开发者手动粘贴。
3. 收集、去重、分类，并与已有 issue 匹配。
4. 起草功能建议 issue，应用前等待明确批准。

## 轨道 4：收集开发经验并提交 PR

运行 `experience-pr` skill，把 fork 相对上游 `docs/` 差异对应的持久、可复用的经验固化，
并作为文档 PR 提交到上游 `FoloToy/ai-passport` 项目。

1. 确认开发者同意开始收尾（涉及项目私有内容）。
2. 确认已有可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）；否则把草案交给开发者手动粘贴。
3. 收集 fork 相对上游 `docs/` 差异里的可复用经验并分流（通用、上游也受益的经验回上游；
   纯 fork 定制按 `fork-guide.md` 留在 fork），保存为
   `docs/experiences/<username>/` 下的一个新的条目文件（按条目内容概要命名，小写连字符，并配
   `.zh_CN.md`，按贡献开发者的 GitHub 用户名分组），
   从 [经验索引](experience-notes.md) 链接它，放在以最新上游 `main` 为基线的独立分支上，
   确保当前 checkout 不被改动。
4. 把变更交给开发者审查，然后在获得明确批准后再 commit、push 到 fork、并向上游 `FoloToy/ai-passport` 开 PR。

## 共同的安全与同意门槛

轨道 1 是设备检查，不需要 skill，也不需要 GitHub 通道。轨道 2–4 遵守下面这些不可协商的规则：

- 开始前确认同意；本工作涉及项目私有内容。
- 任何提交前确认已有可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）；若都不可用，则生成内容供手动粘贴并停止。
- 在开发者审查并授权之前，不提交（issue 或 PR）。
- 不在开发者当前分支上提交或修改；PR 变更放在独立分支或 worktree 上承载。
- 永远不包含凭证、设备 QR 密钥、私密设备链接、个人数据或未脱敏日志。

## 相关文档

- 固件发布：[publish-to-community.md](publish-to-community.md)
- 应用档案：[`../../plays/README.md`](../../plays/README.md)
- 提交 issue：[file-issues.md](file-issues.md)
- 开发经验：[experience-notes.md](experience-notes.md)
- issue 与贡献规则：[../contribution/commit-and-pr.md](../contribution/commit-and-pr.md)
