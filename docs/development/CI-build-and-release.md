# 自动构建与发布（CI / Build & Release）

本仓库提供一套基于 GitHub Actions 的自动构建与发布流水线，用于在打 tag 时自动编译固件并发布 Release。

> 本文件随 CI 工作流维护在 `ci/build-firmware` 分支，与上游 main 的软件设计文档分开管理。

## 触发条件

- **push tag**：当向仓库推送 tag（如 `v1.0.0`、`v0.1.0-feature/xxx`）时触发自动构建，并在构建成功后自动创建 Release（带固件产物）。
- **workflow_dispatch**：可在 GitHub Actions 页面手动触发（用于调试/预发布验证）。

> 平时 push 到分支（非 tag）**不会**触发构建；只有打 tag 才会。

## 流水线做了什么

1. **ccache 缓存恢复**：使用 `actions/cache` 缓存编译中间产物（`.ccache`），二次编译大幅提速。缓存 key 含 ref 与 commit sha，7 天保留（GitHub Actions 默认策略）。
2. **编译**（ESP-IDF 5.5.3 / esp32c3）：`idf.py -D SDKCONFIG_DEFAULTS=sdkconfig.defaults build`。由 `sdkconfig.defaults` 启用自定义分区表（`CONFIG_PARTITION_TABLE_CUSTOM=y`、`CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions.csv"`、`CONFIG_PARTITION_TABLE_FILENAME="partitions.csv"`），编译时读取 `partitions.csv`。
3. **合并完整固件**：用 `idf.py merge-bin` 把 bootloader + partition-table + app 及其他分区（nvs、phy_init）按本次构建的分区布局合并为**可直刷的完整固件** `build/FoloToy-AI-Passport-full.bin`（`--fill-flash-size 8MB`，固定 8MB）。
4. **发布**：
   - **tag 触发** → 用 `softprops/action-gh-release@v2` 创建 GitHub Release，附带产物 `FoloToy-AI-Passport-full.bin`。
   - 分支触发 → 上传为 Actions artifact（不创建 Release）。

## 产物

- `FoloToy-AI-Passport-full.bin`：合并后的完整固件，可直接烧录（唯一产物）。

## 在线烧录

使用浏览器在本机完成写入与校验，固件不会上传服务器。打开 **在线刷机工具**：

`https://ai-passport.folotoy.cn/tools/web-flasher/`

步骤：连接设备（USB JTAG/serial debug unit）→ 选择本 Release 的合并固件 `FoloToy-AI-Passport-full.bin` → 选择波特率（如 460800）→ 开始写入。目标是 8MB Flash 板卡，无需其它参数。

## 相关文件

- `.github/workflows/build-firmware.yml`：本流水线定义。
- 详见 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`（硬件/烧录细节）。
