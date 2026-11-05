<p align="right">
  <strong>简体中文</strong> · <a href="doc-conventions.md">English</a>
</p>

# 文档规范（Doc Conventions）

本规范适用于人类贡献者和 AI agent。文档与代码一样接受 review、自动检查和事实校验；文档是否权威由其职责决定，与作者是人还是 AI 无关。

## 文档语言与文件布局

- 语言豁免仅适用于根 README：`README.md` 可以使用任意语言，任意数量的 `README.<tag>.md` 版本可以并存；出现多语言版本时，所有语言链接必须指向真实存在的文件，并保持双向互链。
- 其它所有维护中的 Markdown 文档，包括 `AGENTS.md` 等根目录 Markdown，其默认 `name.md` 路径必须使用英文；简体中文使用配对的 `name.zh_CN.md`。
- 上述非根目录配对文件顶部必须提供互相切换的链接，并在同一次变更中保持章节、事实、示例、安全提示和链接一致。
- 根目录之外的英文默认页不得混入中文正文；语言切换中的“简体中文”标签是唯一例外。
- 新增、移动或删除任一语言文件时，必须同步处理配对文件和所有索引。
- `tools/check_repo.py` 与 CI 会拒绝除根 README 外缺少配对文件、缺少切换链接或英文默认页包含中文正文的变更。

### GitHub 社区文档链接

GitHub 还会在文件页面之外展示 `.github/` 中的 `CONTRIBUTING`、`CODE_OF_CONDUCT`、
`SECURITY` 和 `SUPPORT` 文档。两种语言的切换均须放在第一个非空行、HTML 块之外，
使用仓库根路径的 Markdown 链接，例如 `[简体中文](/.github/CODE_OF_CONDUCT.zh_CN.md)`；其他仓库内链接也使用
根路径，例如 `/docs/README.md`。HTML 语言切换中的裸文件名在仓库首页可能丢失
`.github/` 目录。不要把上游仓库所有者或 `main` 写死在这些文档链接中，根相对
链接会保留当前仓库与分支。参见 GitHub 的
[相对链接规则](https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax#relative-links)。
仓库门禁会检查这些社区文档链接；上线后还需从首页和单文件页面实际点击中英文
双向切换。

## 随仓库引入的第三方文档

把第三方组件复制到仓库时，应保留其上游原始文档。如需豁免其中 Markdown
的本地链接和双语检查，在 [`tools/check_repo.py`](../../tools/check_repo.py)
的 `VENDORED_DOC_ROOTS` 中显式登记组件目录。该列表默认为空。例如，引入位于
`components/vendor_audio` 的组件后，可登记：

```python
VENDORED_DOC_ROOTS: tuple[str, ...] = (
    "components/vendor_audio",
)
```

登记项必须是已存在的具体组件目录，使用相对仓库根目录的路径和 `/` 分隔符。
空路径、仓库根目录、绝对路径、`.` 或 `..` 路径段以及符号链接目录均会被拒绝。
匹配按完整路径段进行，因此此项不会豁免 `components/vendor_audio_extra`。
链接到登记目录之外的文件也不豁免。不要登记 `components`、`docs` 等包含自有
内容的大范围目录。

只有登记目录内的上游 Markdown 跳过 `check_markdown_links` 和
`check_document_languages`。这些文件仍参加正常的仓库扫描：敏感凭证模式、
未脱敏的设备二维码链接和合并冲突标记仍会导致校验失败。不得通过 `.gitignore`、
`git_files()` 或 `text_files()` 过滤来实现这些豁免。

组件的上游来源 URL、固定版本或提交、许可证以及本地修改，应记录在豁免目录外、
由项目维护的中英文配对文档中。项目自己编写的集成说明仍须遵守正常的双语和链接
规则。添加豁免不要求改写或翻译第三方原始文档。

## 按任务加载上下文

- 所有任务只强制先读仓库根 `AGENTS.md`。
- 按 `AGENTS.md` 的路由表读取与当前修改相关的文档和源码，不默认读取全部 README。
- 产品总览见 `docs/README.md`；需要发现其它文档时再读取 `docs/README.md`。
- 修改某个事实时，更新其权威来源以及直接引用该事实的文档，不复制新的事实源。

## 文档职责

- **上游基线文档**：描述 AI Passport 硬件、BSP、基线 demo、工程约束和验收方法，可以且应当进入上游。
- **通用协作与工程规范**：描述代码风格、测试、提交、CI 和 AI 工作流，可以进入上游，但必须以本仓库真实工具和流程为准。
- **fork 项目文档**：只描述某个 fork 的产品需求、业务逻辑或素材，放在 fork 自己的根 `README.md` 或 `docs/assets/`，不回合上游。

## 放置位置

- 仓库根目录仅保留 AI 入口（`AGENTS.md` 及其维护中的简体中文版 `AGENTS.zh_CN.md`）、可选的 fork README 文件、许可证、构建清单和 ESP-IDF 配置。
- 项目说明与变更历史放入 `docs/`，按协作、开发、硬件和软件职责分类。
- GitHub 自动识别的社区文档、模板、Issue Form 和 workflow 放入 `.github/`。
- 可复用资源放入 `assets/`，项目 skill 放入 `skills/`，自动化脚本放入 `tools/`。
- 仓库检查会拒绝新增其它根目录 Markdown。不要为了显眼而把文档放根目录，应从 `docs/README.md` 建立入口。
- 根 README 的内容范围跟分支模型一致：`main` 分支的 README 介绍托管在各分支上的应用；如果直接在 `main` 上开发，则改为介绍 `main` 自己的功能。其它分支的根 README 只介绍本分支的应用，不得列出或介绍其它分支的应用。

不要创建只有目录说明而没有实际用途的空骨架。新增文档必须在 `docs/README.md` 或所属目录索引中登记；删除或移动文档时同步更新所有链接。

## 变更日志所有权

- 普通功能、应用和文档 PR 不修改 `docs/CHANGELOG.md` 与 `docs/CHANGELOG.zh_CN.md`。用户可见行为、兼容性和发布流程影响写入 PR 正文，并同步更新对应的产品或应用权威文档。
- 发布准备期间，发布负责人检查自上一版本以来已合并的 PR，只保留用户可见变化，并在创建 tag 前同时更新两种语言的变更日志。
- 发布准备变更把本次发布条目归入带版本号和日期的标题，并留下新的 `Unreleased` 章节。现有 `Unreleased` 条目是下次发布的待核对输入，必须与实际合并内容核对，不能直接照搬。

## 写作与维护

- 中文版正文使用全角标点；代码、命令、路径、URL、标识符、YAML/JSON 字段和英文句子保持 ASCII 格式。
- 注释和文档解释“为什么”、边界、失败方式和验证方法，避免重复源码可以直接表达的内容。
- 直接陈述产品事实和公开硬件接口，不加入来源及资料开放状态说明。
- 中英文双语文档必须保持章节和事实一致；只修改一侧时，在交付中明确另一侧是否需要同步。
- 能由脚本检查的规则必须落实到 `tools/` 和 CI，不能只写在文档里。

## 内容安全

凭证、令牌、密钥、授权文件、私钥、个人数据和内部 endpoint 不得进入仓库或 Git 历史。提交前运行 `./tools/validate.sh --static`；发现疑似凭证时停止提交并改用环境变量或系统钥匙串。误提交后必须私下报告并轮换凭证，仅删除工作区文件不能消除 Git 历史中的泄露。

设备二维码链接的 `s`、`k` 参数属于敏感标识，不得写入代码、配置、文档、示例、测试、日志或 commit message。文档只能使用明显脱敏的形式：

```text
https://ai-passport.folotoy.cn/trae/?s=<secret>&k=<key>
```

## 文件操作安全

- 保留用户已有修改和未跟踪文件。
- 删除用户文件或来源不明的生成物时使用系统回收站；没有安全回收方式时先询问用户。
- 工具可以清理由自身通过 `mktemp` 创建且已验证路径的临时目录。
- Git 分支、tag 和远端引用不是普通文件；删除它们前仍需确认目标和用户意图。
