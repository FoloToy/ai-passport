<p align="right">
  <strong>简体中文</strong> · <a href="protected-flash-layout.md">English</a>
</p>

# 受保护的 Flash 布局

本仓库保留工厂所有的 Flash 区域，避免二创固件覆盖设备身份或
Recovery 镜像。模板不再提供按键持续按住后进入 Recovery 的 bootloader
入口。

## 必须保持的布局

二创项目必须同时保留：

- ESP32-C3、8 MB Flash、ESP-IDF 5.5.3。
- 从 `0x0` 开始的合并 ESP 镜像，固定产物为
  `build/FoloToy-AI-Passport-full.bin`。
- 位于 `0x10000` 的主应用镜像，不得超过 `0x300000` 字节。
- `cardid`：data/NVS，地址 `0x356000`，大小 `0x4000`。
- `recovery`：app/test，地址 `0x700000`，大小 `0x100000`。
- 有效的分区表 MD5，且所有分区不得与两个保护区重叠。
- 社区产物不得包含任何单台设备的 `cardid` 数据，也不得携带替换
  Recovery 的数据。

应用可以新增资源分区，但不得覆盖保护区。必需的资源分区必须打入
合并产物，不得只在分区表中声明空分区。

## Recovery 边界

`recovery` 分区仍为工厂镜像保留，但本模板不再包含
`recovery_boot_hook`，也不定义任何功能键进入 Recovery 的操作。
只保留分区布局，不代表二创镜像仍可通过小程序安装或恢复。

## 强制验证

执行：

```bash
./tools/validate.sh --firmware
```

脚本会在隔离目录构建，生成合并镜像，验证 bootloader、分区表与应用的
偏移，解析分区表并检查 MD5、保护范围和 3 MB 应用上限，同时拒绝保护
分区数据入包。CI 执行同一门禁。该命令失败时不得发布。

只上传 `build/FoloToy-AI-Passport-full.bin`。名称相近的应用单镜像
`build/FoloToy-AI-Passport.bin` 不包含经完整验证的布局。

## 开发烧录安全

已写入设备身份的机器严禁执行 `idf.py erase-flash`，否则会同时破坏单机身份与
工厂 Recovery 镜像。优先使用不会写保护分区镜像的分段 `idf.py flash`。
只有单文件的字节范围在 `cardid` 之前结束时，从 `0x0` 直接写入才安全；
若合并产物包含位于 `cardid` 之后的资源分区，就不得对已写身份的设备做
单文件直刷。
