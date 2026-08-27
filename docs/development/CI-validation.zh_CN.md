<p align="right">
  <strong>简体中文</strong> · <a href="CI-validation.md">English</a>
</p>

# PR 自动验证（CI Validation）

`.github/workflows/ci.yml` 在 Pull Request、`main` push 和手动触发时运行，与本地共用 `tools/validate.sh`。

## Jobs

- **Repository and host checks**：校验 Markdown 相对链接、GitHub Action 完整 SHA、Issue Form 基本结构、依赖锁文件、冲突标记和疑似敏感信息；随后运行 `actionlint` 与 host tests。
- **ESP-IDF 5.5.3 firmware**：在全新隔离的构建/配置目录中，使用 ESP-IDF 5.5.3 / ESP32-C3 运行 `./tools/validate.sh --firmware`，验证编译、0x0 合并固件及其中三个映像的偏移和内容，并保留 7 天 Actions artifact。

工作流只有 `contents: read` 权限，不使用仓库 secrets，因此可以安全验证来自 fork 的 PR。所有 GitHub Actions 固定到完整 commit SHA；升级时必须核对官方版本、更新 SHA 注释并运行 `actionlint`。

## 本地复现

```bash
./tools/validate.sh --static
source <ESP-IDF-v5.5.3-路径>/export.sh
./tools/validate.sh --firmware
```

未安装 ESP-IDF 5.5.3 时先按[环境引导](environment-setup.zh_CN.md)搭建。CI
失败应先在本地运行相同模式。不要在 workflow 中复制另一套构建或校验命令。
