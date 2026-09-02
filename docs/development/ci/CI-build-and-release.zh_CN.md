<p align="right">
  <strong>简体中文</strong> · <a href="CI-build-and-release.md">English</a>
</p>

# 自动构建与发布（CI / Build & Release）

本仓库提供一套基于 GitHub Actions 的自动构建与发布流水线，用于在打 tag 时自动编译固件并发布 Release。

本文件与 `.github/workflows/build-firmware.yml` 一同维护，工作流行为变化时必须同步更新。

## 触发条件

- **push tag**：当向仓库推送 tag（如 `v1.0.0`、`v0.1.0-feature/xxx`）时触发自动构建，并在构建成功后自动创建 Release（带固件产物）。
- **workflow_dispatch**：可在 GitHub Actions 页面手动触发（用于调试/预发布验证）。

> 平时 push 到分支（非 tag）**不会**触发构建；只有打 tag 才会。

## 流水线做了什么

1. **ccache 缓存恢复**：使用 `actions/cache` 缓存编译中间产物（`.ccache`），二次编译大幅提速。缓存 key 含 ref 与 commit SHA；缓存保留时间以仓库的 GitHub Actions 设置为准。
2. **编译与验证**（ESP-IDF 5.5.3 / esp32c3）：运行与本地相同的 `./tools/validate.sh --firmware`。脚本使用 `sdkconfig.defaults` 和 `partitions.csv` 构建固件，再执行 `idf.py merge-bin`。
3. **验证完整固件**：脚本逐字节确认 bootloader、partition-table 和 app 位于 `0x0`、`0x8000` 和 `0x10000`，确认 `flash_args` 使用 8 MB Flash，并完整检查小程序 BLE 兼容契约，最后输出 `build/FoloToy-AI-Passport-full.bin`。
4. **上传 artifact**：每次成功构建都上传 `FoloToy-AI-Passport-full.bin`。普通分支只有从该分支手动运行 `workflow_dispatch` 才会构建；普通 push 不触发。
5. **发布 tag**：tag 构建完成后，独立 release job 下载上述 artifact，并创建 GitHub Release。

构建 job 只有 `contents: read` 权限；仅 release job 在 tag 发布时获得 `contents: write`。所有 Action 均固定到完整 commit SHA，行尾注释保留对应发布版本，升级时需同时核对 SHA 与版本。

## 产物

- `FoloToy-AI-Passport-full.bin`：合并后的完整固件，可直接烧录（唯一产物）。

## 在线烧录

使用浏览器在本机完成写入与校验，固件不会上传服务器。打开 **在线刷机工具**：

`https://ai-passport.folotoy.cn/tools/web-flasher/`

步骤：连接设备（USB JTAG/serial debug unit）→ 选择本 Release 的合并固件 `FoloToy-AI-Passport-full.bin` → 选择波特率（如 460800）→ 开始写入。目标是 8MB Flash 板卡，无需其它参数。

## Release 标题

当本仓库从同一棵源码树发布多个不同应用时，只有版本号看不出这个 Release 是哪个应用。给每个 tag 起一个
同时带版本与应用名的名字，并确保 Release 标题两者都显示。

- **Tag 命名约定**：按 `v<版本>-<应用名>` 小写连字符命名，例如 `v0.1.0-voice-keychain`、
  `v1.0.0-pocket-pomodoro`。`<应用名>` 是该 Release 构建的应用（见
  `plays/<username>/<app-name>/` 档案命名）。多应用共享同一棵树时，只写版本号的 tag 会有歧义。
- **发布成功后，核对 Release 标题**：workflow 会把标题设为 tag 名，因此命名正确的 tag 本身就显示成
  `v0.1.0-voice-keychain`。若 tag 没带应用名，或标题一眼看不出是哪个应用，就编辑该 Release
  （GitHub：`Edit release`；GitLab：编辑 tag），让标题为 `<版本> <应用名>`，例如 `v0.1.0 Voice
  Keychain`。快速扫一遍 Release 列表，就能区分每个 Release 是哪个应用。
- **标题与 tag 保持一致**：用 `<版本>-<应用名>`，让应用名在 tag 列表和 Release 列表里都可见。不要只靠
  人类可读的正文承载应用名。

## Release 说明

tag 触发的 Release 只有在合并固件与它的 Release 说明一起发布时才完整。发布 Release 后，要写一份
说明，向可能没读过仓库的用户解释这次构建。覆盖三块：

- **功能（What's new / 功能）**：本次 Release 相对上一版新增或变更的功能、行为或修复。面向用户，
  不是 commit 日志。
- **方法（How to build / 方法）**：如何生成并校验合并固件（`./tools/validate.sh --firmware` 或
  `idf.py build`），以及要烧录的产物文件（从 `0x0` 烧录的 `FoloToy-AI-Passport-full.bin`）。
- **使用（How to use / 使用）**：如何烧录（上方在线刷机工具），以及本次 Release 的关键交互或硬件
  要求。

用英文写 Release 说明（项目双语时再配一份简体中文），并在 GitHub/GitLab Release 上链接它们。对
用户可见的行为，保持与 `docs/CHANGELOG.md` 一致。

## 相关文件

- `.github/workflows/build-firmware.yml`：本流水线定义。
- 详见 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`（硬件/烧录细节）。
