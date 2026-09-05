<p align="right">
  <strong>简体中文</strong> · <a href="CI-validation.md">English</a>
</p>

# PR 自动验证（CI Validation）

两个工作流验证 PR：`.github/workflows/static-checks.yml` 与
`.github/workflows/firmware-checks.yml`。两者都在 Pull Request、`main` push 和手动触发时运行，
与本地共用 `tools/validate.sh`。

## 工作流

- **Static checks**（`static-checks.yml`）：校验英文默认双语 Markdown、相对链接、GitHub Action
  完整 SHA、Issue Form 基本结构、依赖锁文件、冲突标记和疑似敏感信息；随后运行 `actionlint` 与
  host tests。它在每个 pull request 上运行。
- **Firmware checks**（`firmware-checks.yml`）：在全新隔离的构建/配置目录中，使用 ESP-IDF 5.5.3 /
  ESP32-C3 运行 `./tools/validate.sh --firmware`，验证编译、0x0 合并固件的偏移与内容，并保留 7 天
  Actions artifact。

## 路径过滤

`firmware-checks.yml` 忽略不会影响固件构建的路径：`docs/**`、`plays/**`、`skills/**` 与 `*.md`。
因此纯文档的 pull request 只跑 static checks、跳过固件构建。`assets/**` **故意不**忽略：`assets/`
存放可复用的字体、图片、音乐与音效，未来可能被嵌入固件，因此任何资源变更都必须触发固件构建。

## 路径过滤与 required check 的兼容性

被路径过滤跳过的 workflow 不会为那个 job 产生 check run。若未来把 firmware job 设为 **required**
状态检查，纯文档的 pull request 将没有 firmware check 可满足，从而可能阻塞合并。在把 firmware job
设为 required 之前请谨记这一点；若确需 required，必须重新审视路径过滤（例如把 job 拆出一个
文档专用的 pass/fail 桩，或改用单一无条件 workflow）。

两个工作流都只有 `contents: read` 权限、不使用仓库 secrets，因此可以安全验证来自 fork 的 PR。
所有 GitHub Actions 固定到完整 commit SHA；升级时必须核对官方版本、更新 SHA 注释并运行 `actionlint`。

## 本地复现

```bash
./tools/validate.sh --static
source <ESP-IDF-v5.5.3-路径>/export.sh
./tools/validate.sh --firmware
```

未安装 ESP-IDF 5.5.3 时先按[环境引导](../engineering/environment-setup.zh_CN.md)搭建。CI
失败应先在本地运行相同模式。不要在 workflow 中复制另一套构建或校验命令。
