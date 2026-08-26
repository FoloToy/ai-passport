<p align="right">
  <strong>简体中文</strong> · <a href="after-release.md">English</a>
</p>

# 发布后收尾

一旦固件已发布到 AI Passport 社区，就把发布闭环收尾，把它转化成三类后续工作。发布本身见
[publish-to-community.md](publish-to-community.md)；本页是发布**之后**做什么的总览。

收尾沿三条独立轨道进行。三者都由仓库内的 skill 驱动，都需要相同的安全与同意门槛，都不负责
发布固件。

## 轨道 1：把应用归档到 plays

发布后，询问开发者是否把该应用归档到仓库的 `plays/` 应用档案。若同意，在 `plays/<app-name>/`
下生成该应用的 AI 功能总结（双语 `README.md` / `.zh_CN.md`），并添加封面图
`plays/<app-name>/<app-name>-cover.<webp|png|jpg>`。只提交总结与封面；**不要**在这里存固件
`.bin`。见 [`../../plays/README.md`](../../plays/README.md)。

生成总结前，先检查 `main` 分支和当前分支**根目录**的 README（`plays-archive` skill）：若有 README，
把它合并进总结；若没有，直接总结。归档完成后对每个分支**各自处理**：没有根 README 的分支创建它，
已有根 README 的分支提示开发者更新它。

## 轨道 2：收集建议并提交 issue

运行 `issue-suggestions` skill，收集发布固件的开发者本人的改进点，把有价值的整理成功能
建议 issue，提交到上游 `FoloToy/ai-passport` 项目。

1. 确认开发者同意开始收尾（涉及项目私有内容）。
2. 确认已有可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）；否则把草案交给开发者手动粘贴。
3. 收集、去重、分类，并与已有 issue 匹配。
4. 起草功能建议 issue，应用前等待明确批准。

## 轨道 3：收集开发经验并提交 PR

运行 `experience-pr` skill，把 fork 相对上游 `docs/` 差异对应的持久、可复用的经验固化，
并作为文档 PR 提交到上游 `FoloToy/ai-passport` 项目。

1. 确认开发者同意开始收尾（涉及项目私有内容）。
2. 确认已有可用的 GitHub 通道（GitHub MCP、GitHub skill 或 `gh`）；否则把草案交给开发者手动粘贴。
3. 收集 fork 相对上游 `docs/` 差异里的可复用经验并分流（通用、上游也受益的经验回上游；
   纯 fork 定制按 `fork-guide.md` 留在 fork），保存为 `docs/experiences/` 下的一个新的条目文件（命名为
   `<unixtime>_<commit-sha>.md`，并配 `.zh_CN.md`），从 [经验索引](experience-notes.md)
   链接它，放在以最新上游 `main` 为基线的独立分支上，确保当前 checkout 不被改动。
4. 把变更交给开发者审查，然后在获得明确批准后再 commit、push 到 fork、并向上游 `FoloToy/ai-passport` 开 PR。

## 共同的安全与同意门槛

每条轨道都遵守下面这些不可协商的规则：

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
