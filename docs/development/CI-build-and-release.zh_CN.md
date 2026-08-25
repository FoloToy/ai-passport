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
3. **验证完整固件**：脚本逐字节确认 bootloader、partition-table 和 app 位于 `0x0`、`0x8000` 和 `0x10000`，确认 `flash_args` 使用 8 MB Flash，并输出可从 `0x0` 直刷的 `build/FoloToy-AI-Passport-full.bin`。
4. **上传 artifact**：每次成功构建都上传 `FoloToy-AI-Passport-full.bin`。普通分支只有从该分支手动运行 `workflow_dispatch` 才会构建；普通 push 不触发。
5. **发布 tag**：tag 构建完成后，独立 release job 下载上述 artifact，并创建 GitHub Release。

构建 job 只有 `contents: read` 权限；仅 release job 在 tag 发布时获得 `contents: write`。所有 Action 均固定到完整 commit SHA，行尾注释保留对应发布版本，升级时需同时核对 SHA 与版本。

## 产物

- `FoloToy-AI-Passport-full.bin`：合并后的完整固件，可直接烧录（唯一产物）。

## 在线烧录

使用浏览器在本机完成写入与校验，固件不会上传服务器。打开 **在线刷机工具**：

`https://ai-passport.folotoy.cn/tools/web-flasher/`

步骤：连接设备（USB JTAG/serial debug unit）→ 选择本 Release 的合并固件 `FoloToy-AI-Passport-full.bin` → 选择波特率（如 460800）→ 开始写入。目标是 8MB Flash 板卡，无需其它参数。

## 相关文件

- `.github/workflows/build-firmware.yml`：本流水线定义。
- 详见 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`（硬件/烧录细节）。
