<p align="right">
  <strong>简体中文</strong> · <a href="ble-recovery-compatibility.md">English</a>
</p>

# 小程序 BLE 固件兼容规范

本仓库是二创固件模板。任何基于它开发的应用，都必须保持可由 AI Passport
小程序通过官方默认固件预装的永久 Recovery 安装。

## 安装原理

玩法应用本身不实现 BLE 刷机服务。用户关机后按住上键开机 5 秒，自定义
bootloader 跳转到 `0x700000` 的工厂预装 Recovery。Recovery 提供 FFF0–FFF4
BLE 服务，验证设备后接收应用与资源分区，保护设备身份，写入兼容分区表，
最后启动新应用。

因此，社区固件是 Recovery 的安装输入，不是 Recovery 的替代品。如果设备的永久
Recovery 已被擦除，必须先走官网 USB 恢复流程。

## 必须保持的契约

二创项目必须同时保留：

- ESP32-C3、8 MB Flash、ESP-IDF 5.5.3。
- 从 `0x0` 开始的合并 ESP 镜像，固定产物为
  `build/FoloToy-AI-Passport-full.bin`。
- 位于 `0x10000` 的主应用镜像，不得超过 `0x300000` 字节。
- `cardid`：data/NVS，地址 `0x356000`，大小 `0x4000`。
- `recovery`：app/test，地址 `0x700000`，大小 `0x100000`。
- `bootloader_components/recovery_boot_hook/` 下的 bootloader hook，上键持续
  5 秒后进入 Recovery。
- 有效的分区表 MD5，且所有分区不得与两个保护区重叠。
- 社区产物不得包含任何单台设备的 `cardid` 数据，也不得携带替换 Recovery
  的数据。

应用可以新增资源分区，但不得覆盖保护区。必需资源分区必须打入合并产物，
不得只在分区表声明一个空分区。

## 强制验证

执行：

```bash
./tools/validate.sh --firmware
```

脚本会在隔离目录构建，生成合并镜像，验证 bootloader、分区表与应用的偏移，
解析分区表并检查 MD5、保护范围和 3 MB 应用上限，拒绝保护分区数据，同时
确认 Recovery boot hook 已链接。CI 执行同一门禁。该命令失败时不得发布。

只上传 `build/FoloToy-AI-Passport-full.bin`。名称相近的应用单镜像
`build/FoloToy-AI-Passport.bin` 无法通过小程序兼容检测。

## 开发烧录安全

已写入设备身份的机器严禁执行 `idf.py erase-flash`，否则会同时破坏单机身份与永久
Recovery。优先使用小程序安装，或使用不会写保护分区镜像的分段 `idf.py flash`。
只有单文件的字节范围在 `cardid` 之前结束时，从 `0x0` 直接写入才是安全的；
若合并产物包含位于 `cardid` 之后的资源分区，就不得对已写身份的设备做单文件直刷。
