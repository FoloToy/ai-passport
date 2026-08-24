# 自动构建与发布（CI / Build & Release）

本仓库提供一套基于 GitHub Actions 的自动构建与发布流水线，用于在打 tag 时自动编译固件并发布 Release。

## 触发条件

- **push tag**：当向仓库推送 tag（如 `v1.0.0`、`v0.1.0-feature/xxx`）时触发自动构建，并在构建成功后自动创建 Release（带固件产物）。
- **workflow_dispatch**：可在 GitHub Actions 页面手动触发（用于调试/预发布验证）。

> 平时 push 到分支（非 tag）**不会**触发构建；只有打 tag 才会。

## 流水线做了什么

1. **ccache 缓存恢复**：使用 `actions/cache` 缓存编译中间产物（`.ccache`），二次编译大幅提速。缓存 key 含 ref 与 commit sha，7 天保留（GitHub Actions 默认策略）。
2. **编译**（ESP-IDF 5.5.3 / esp32c3）：`idf.py build`。
3. **合并完整固件**：用 `esptool merge_bin` 把 bootloader + partition-table + app 合并为**可直刷的完整固件** `build/FoloToy-AI-Passport-full.bin`（`--chip esp32c3 --flash_size 8MB`）。
4. **发布**：
   - **tag 触发** → 创建 GitHub Release，附带产物（`FoloToy-AI-Passport-full.bin`、`FoloToy-AI-Passport.bin`、`bootloader.bin`、`partition-table.bin`）。
   - 分支触发（若保留）→ 上传为 Actions artifact。

## 产物

- `FoloToy-AI-Passport-full.bin`：合并后的完整固件，可直接烧录。
- `FoloToy-AI-Passport.bin`：应用固件。
- `bootloader.bin`、`partition-table.bin`：分区组件。

## 相关文件

- `.github/workflows/build-firmware.yml`：本流水线定义。
- 详见 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`（硬件/烧录细节）。
