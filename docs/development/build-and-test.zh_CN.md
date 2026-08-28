<p align="right">
  <strong>简体中文</strong> · <a href="build-and-test.md">English</a>
</p>

# 构建与验证

使用 ESP-IDF 5.5.3。全新机器或缺少工具链时，先按[环境引导](environment-setup.zh_CN.md)完成安装。

## 开发构建

```bash
source <ESP-IDF-v5.5.3-路径>/export.sh
idf.py --version              # 必须输出 ESP-IDF v5.5.3
idf.py set-target esp32c3     # 全新 checkout 或切换 target 后运行
idf.py build                  # 增量应用构建
idf.py flash monitor          # 仅用于增量开发烧录与日志监视
idf.py fullclean              # 只删除生成的构建状态
```

`idf.py fullclean` 不能让已有 `sdkconfig` 完整同步变更后的 defaults。需要重新生成 target 或已跟踪 defaults 时，先保留有意的本地设置，再运行 `idf.py set-target esp32c3`。

仓库提交 `dependencies.lock` 以固定 Managed Components 解析结果。修改 `idf_component.yml` 后，必须使用 ESP-IDF 5.5.3 重新生成锁文件、审查版本变化并与 manifest 一起提交。普通构建不应产生无法解释的锁文件差异。

## 主机与静态检查

迭代时可直接运行与硬件无关的测试：

```bash
./tools/run-host-tests.sh
```

交付前运行完整静态门禁：

```bash
./tools/validate.sh --static
```

静态门禁检查仓库结构、双语 Markdown 配对、链接、Action SHA 锁定、Issue Form、敏感内容模式、冲突标记、主机逻辑测试和 workflow 语法。Workflow lint 需要 `actionlint`；使用 `export ACTIONLINT_BIN="$(./tools/install-actionlint.sh)"` 安装已校验版本。`actionlint` 不可用时，仓库检查和主机测试仍执行，workflow lint 报告 `NOT RUN`，静态命令以失败退出。

## 固件交付

交付产物是经过逐字节验证、从 `0x0` 偏移烧录的合并镜像 `build/FoloToy-AI-Passport-full.bin`：

```bash
./tools/validate.sh --firmware
```

固件门禁使用全新的临时构建目录，并从 `sdkconfig.defaults` 生成隔离的 `sdkconfig`。它面向 ESP32-C3 构建，合并 bootloader、partition table 与应用，验证各自偏移和字节，最后只把验证通过的完整镜像复制到 `build/`。

激活 ESP-IDF 5.5.3 后运行完整本地门禁：

```bash
./tools/validate.sh
```

## CI 与发布

- `.github/workflows/static-checks.yml` 在每个 PR 和 `main` push 上运行仓库、主机与 workflow 检查。
- `.github/workflows/firmware-checks.yml` 在 `CMakeLists.txt`、`components/**`、`dependencies.lock`、`main/**`、`partitions.csv`、`sdkconfig.defaults`、`tools/validate.sh`、`tools/verify_firmware.py` 或该 workflow 自身变化时构建；同时支持手动触发，并把验证镜像保留七天。
- `.github/workflows/build-firmware.yml` 在手动触发或 tag 时构建同一镜像，仅在 tag 时创建 GitHub Release。

所有 workflow 都调用 `tools/validate.sh`，固件构建统一使用 ESP-IDF 5.5.3，Action 锁定到完整 commit SHA，并且只有 tag release job 获得写权限。若本地与 CI 行为不同，应修复共享脚本或环境，而不是在 YAML 中复制命令。

## 实机验证

构建成功不等于硬件验收。修改 `components/bsp/`、`main/demo_*.c`、`sdkconfig.defaults` 或 `partitions.csv` 时，必须执行硬件指南中受影响子系统的全部验收行。未连接板卡时，把这些行列入 `Unverified`，并报告 `Device tests: NOT RUN`。分别报告 `Build`、`Host tests`、`Device tests` 和 `Unverified`。
