# 构建与验证（Build & Test）

> 定位：公共文档，适用于任何项目，可提上游。

使用 ESP-IDF 5.5.x（已知开发环境 5.5.3）：

```bash
get_idf553                    # 进入仓库的 ESP-IDF 5.5.3 环境
idf.py set-target esp32c3     # 配置目标芯片（fresh checkout 后/换 target 后运行）
idf.py build                  # 编译固件，验证依赖
idf.py flash monitor          # 烧录并打开日志
idf.py fullclean              # 配置过期时清空生成状态（勿用于清理用户源码改动）
```

当前基线含一个可独立运行的纯逻辑测试：

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

改动后至少跑 `idf.py build`（最小自动化检查）+ 适用逻辑测试；涉及物理外设的改动必须在真机运行 README 验收清单，并把"编译通过"与"硬件验证通过"分开记录（禁止把编译通过描述成硬件验证通过）。
