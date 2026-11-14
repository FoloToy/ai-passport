<p align="right">
  <strong>简体中文</strong> · <a href="build-and-test.md">English</a>
</p>

# 构建与验证（Build & Test）

使用 ESP-IDF 5.5.3。全新机器或缺少工具链时，先按
[环境引导](environment-setup.zh_CN.md)完成安装。

> **向设备下载（烧录）新固件前，无需备份设备内部原有固件。** 不要求先读出
> 原固件，也不把原固件备份作为烧录前置条件。烧录会覆盖原固件，不会自动恢复
> 原固件。这不代表用户数据会被保留：如果需要保留已有设置或记录，应事先
> 导出或另行保存。详见[烧录与已存数据](firmware-layout.zh_CN.md#烧录与已存数据)。

> 固件编译优先运行 `./tools/validate.sh --firmware`。空白设备初始化或有意完整
> 刷新时，把验证通过的 `build/FoloToy-AI-Passport-full.bin` 从 `0x0` 写入；
> 合并镜像可能重置 NVS，需要保留已有 NVS 状态时使用分段 `idf.py flash`。
> `idf.py build` 和
> `idf.py flash` 只作为增量开发命令，不作为默认交付方式。

```bash
source <ESP-IDF-v5.5.3-路径>/export.sh
idf.py --version             # 必须输出 ESP-IDF v5.5.3
./tools/validate.sh --firmware # 优先：编译并验证 0x0 合并固件
idf.py set-target esp32c3     # 配置目标芯片（fresh checkout 后/换 target 后运行）
idf.py build                  # 可选：增量 app 编译
idf.py flash monitor          # 可选：增量 app 烧录
idf.py fullclean              # 只清空过期生成状态（勿用于清理用户源码改动）
```

`idf.py fullclean` 不能让已有 `sdkconfig` 完整同步变更后的 defaults。需要重建
target 或已跟踪 defaults 时，先保留有意的本地设置，再运行
`idf.py set-target esp32c3`。

### 加速重复编译

ccache 能在源码需要重新编译时复用已有编译结果。ESP-IDF 5.5.3 默认不启用
ccache。激活 ESP-IDF 后，先确认 ccache 可用，再为单次构建启用（以下命令
同样适用于 Linux/macOS shell 和 Windows 原生 ESP-IDF 终端）：

```text
ccache --version
idf.py --ccache build
```

也可以在当前 Linux/macOS shell 及其子进程中启用，让验证脚本一并使用：

```bash
export IDF_CCACHE_ENABLE=1
idf.py build
```

这是 `idf.py` 选项，不是 `sdkconfig` 或 `menuconfig` 配置。需要在项目或 CI
中重复启用时，显式传入 `--ccache`，或在该构建环境设置
`IDF_CCACHE_ENABLE=1`；不要擅自修改 shell 启动文件。参见
[ESP-IDF 5.5.3 选项定义](https://github.com/espressif/esp-idf/blob/v5.5.3/tools/idf_py_actions/core_ext.py)。

先检查当前缓存配置，不要假定固定路径：

```text
ccache --show-config
ccache --show-stats
```

实际 `cache_dir` 取决于 ccache 版本、平台、配置以及 `CCACHE_DIR`，并不总是
`~/.ccache`。把它放在 `build/` 和临时验证目录之外，清理这些目录时才能保留
缓存。清缓存不是日常构建步骤。只有明确需要清空当前缓存时，才使用保留
配置文件的 `ccache --clear`，而不是删除整个目录；这也会清除共用该缓存的
其他项目的编译结果。参见 [ccache 手册](https://ccache.dev/manual/latest.html)。

Windows 上的杀毒或终端安全软件实时扫描可能拖慢构建，但应先诊断瓶颈。
Microsoft Defender 可使用其
[性能分析器](https://learn.microsoft.com/en-us/defender-endpoint/performance-analyzer-reference)，
分析结果不等于自动建议添加排除项。排除项会降低防护能力，且属于可选措施：
必须按适用安全策略取得用户或管理员批准，再把例外限制在已确认问题的最小
范围内。不要例行排除整个 ESP-IDF 安装目录、工具链目录或工程，也不要关闭
实时防护。

仓库提交 `dependencies.lock` 以固定 ESP-IDF Managed Components 的解析结果。修改 `idf_component.yml` 后必须使用 ESP-IDF 5.5.3 重新生成锁文件、review 版本变化并与 manifest 一起提交；普通构建不应产生未提交的锁文件差异。

固件门禁使用全新的临时构建目录，并从仓库 `sdkconfig.defaults` 生成隔离的 `sdkconfig`。它不会读取或覆盖开发者根目录的 `sdkconfig`。清理临时构建前，先归档已验证固件及匹配的调试产物，再把已验证合并镜像复制到 `build/FoloToy-AI-Passport-full.bin`。门禁同时验证[当前配置的固件布局](firmware-layout.zh_CN.md)：从 `flash_args` 读取镜像偏移，检查分区表 MD5、边界和不重叠，并确认应用从所配置的 app 分区起点开始且未超出分区。允许用户自定义分区布局。

### 保留匹配的崩溃调试产物

每次成功的固件门禁都会在本地保留 `build/firmware/<full-bin-sha256>/`
归档。`manifest.json` 记录完整镜像与 ELF 的 SHA-256、项目／应用／IDF
版本字段、偏移，以及每个保留文件的大小和哈希。归档包含：

- 已校验合并镜像及其应用 ELF、MAP、应用镜像。
- `bootloader/bootloader.bin`、`partition_table/partition-table.bin` 和 `flash_args`。

不重新构建、不写入归档即可复验：

```text
python3 tools/archive_firmware.py verify <archive-directory>
```

对已有构建目录，`python3 tools/archive_firmware.py create <build-directory>`
会执行同样的归档校验，但不会运行 host tests，也不能证明使用了当前源码／
配置；不能代替门禁。归档工具本身不要求激活 ESP-IDF。

工具检查 ELF 的 SHA-256 是否与应用镜像内嵌身份一致，并检查保留的分段
镜像是否与合并镜像一致。分析对应固件崩溃时使用该 ELF，不拿后续重新编译
的 ELF 替代，尤其是版本带 `-dirty` 时。MAP 没有内嵌身份，依靠同次构建
留存和清单哈希保护；固件／ELF 与其他保留文件逐字节一致时，重复归档复用
首份已验证归档及 MAP，因为临时构建路径可能改变 MAP 内容。不覆盖冲突归档。

额外自定义分区镜像不作为独立文件留存，但其内容仍可能包含在合并镜像中；
这**不是**完整的分段烧录包，也不保证已脱敏。用户特定分段烧录需要的额外
匹配镜像，应在审核内容后另行保留。把 `flash_args` 当作数据，不作为 shell
脚本执行。

验证失败可能仍保留旧 `build/FoloToy-AI-Passport-full.bin` 和历史归档，
不能将它们当成本轮失败构建的新产物。交接时给出实际成功归档路径和完整
镜像哈希；哈希一致不代表硬件通过验收，也不能证明发布者可信。

`build/` 仍被 Git 忽略。固件与调试文件可能含内嵌凭证或其他私密数据，
不能自动提交或上传。现有 CI／发布流程仍只上传其配置的产物，不上传这些
调试包；runner 删除时，本地归档也会消失。扩大留存／上传范围前须明确审核
内容和访问权限。构建／归档命令不会烧录设备。

当前基线含一个可独立运行的纯逻辑测试：

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

静态门禁还将真实 BSP 和 demo 实现与轻量平台桩编译在一起，故障注入覆盖任务退出交接与停止重试、录音失败、Wi-Fi/BLE 启动回滚、按键分配与 ADC 错误、LVGL 初始化锁与重试、codec 打开/休眠/唤醒恢复。只需主机 C 编译器和 Python，不依赖 ESP-IDF 或已下载的 Managed Components。这些测试不代表真实时序、电气行为或设备兼容性；固件门禁会使用锁定依赖进行编译。

统一验证入口：

```bash
./tools/validate.sh --static    # 仓库一致性、字形覆盖、workflow、文档链接、敏感信息、host tests
./tools/validate.sh --firmware  # ESP-IDF build、merge-bin、偏移与当前布局校验
./tools/validate.sh             # 完整验证
```

完整验证要求预先激活 ESP-IDF 5.5.3。CI 与本地使用同一脚本；若 CI 和本地行为不同，应先修复脚本或环境，而不是维护两份命令。

涉及物理外设的改动必须在真机运行硬件指南验收清单，并把“编译通过”与“硬件验证通过”分开记录。

社区只能上传验证通过的 `build/FoloToy-AI-Passport-full.bin`，不得上传应用单镜像
`build/FoloToy-AI-Passport.bin`，后者不包含完整且经校验的固件布局。
