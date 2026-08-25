# 怎么换宠物 / 给图命名

宠物资源是**数据驱动**的：美术把 PNG 丢进 `pets/`，跑一条脚本，固件里就多了一只宠物。
不用改任何 C 代码。

## 目录

```
pets/                 ← 只动这里（美术原图，AI 出的像素图）
  walk_1_r_moveforward.png
  jump_2_l_sprintup.png
  sleep_1_r_idle.png
  groom_2_r_idle.png
  background.png       ← 可选：整屏背景（240x320）

tools/
  png2lvgl.py          ← 单张 PNG → LVGL9 C 数组（抠背景/镜像/裁切/缩放）
  prep_pet.py          ← 批量调用上面，并汇总生成 pet_manifest.h

main/pet/             ← 自动生成，勿手改
  pet_walk_1.c/.h ...  ← 每只动作每帧一个
  pet_bg.c/.h          ← 仅当 pets/background.png 存在时生成
  pet_manifest.h       ← 行为总表（demo 运行时只读这张）
```

换宠物 = 替换 `pets/` 里的图 → 跑脚本 → 重新编译。

## 命名

```
<动作>_<第几帧>_<朝向>_<运动状态>.png
```

- **动作**：只是分组和显示名，不决定行为。`walk` `jump` `sleep` `groom` `sit` `peck` `fly` 都行。
- **第几帧**：从 1 开始的整数。
- **朝向**：表示这个图面向哪边 `r` 朝右，`l` 朝左（**l 会自动水平镜像，不用画两张**）。
- **运动状态**：只描述"画面怎么动"，跟动物无关：
  `idle` 不动 / `moveforward` 慢走 / `sprintforward` 快走 / `moveup` 小跳 / `sprintup` 大跳。

示例：

```
walk_1_r_moveforward.png    走路第1帧，朝右，水平慢移
jump_3_l_sprintup.png      跳跃第3帧，朝左(自动镜像)，垂直大弧
sleep_2_r_idle.png         睡觉第2帧，原地循环
groom_1_r_idle.png         舔毛第1帧，原地循环
```

**为什么动作名不决定行为**：猫"舔毛"标 `idle`、小鸡"啄米"也标 `idle`，代码只认 `idle`（原地）。
加小熊时 `sit_1_r_idle.png` 自然就是原地，零改动。

运动状态的物理参数（想调速度 / 跳多高，改 `png2lvgl.py` 的 `MOTION_PRESETS` 一处即可）：

| 字段 | 怎么动 | 水平/拍 | 垂直 | 换帧 | 静止 |
|---|---|---|---|---|---|
| `idle` | 不动 | 0 | 0 | 350ms | 3000ms 后转身 |
| `moveforward` | 水平慢移 | 1px | 0 | 130ms | 走到撞墙 |
| `sprintforward` | 水平快移 | 3px | 0 | 90ms | 走到撞墙 |
| `moveup` | 小跳 | 1px | 6px | 120ms | 走到撞墙 |
| `sprintup` | 大跳 | 2px | 12px | 110ms | 走到撞墙 |

## 抠背景

AI 出的图背景一般是洋红 `#F74859`，脚本自动抠成透明，你不用自己擦。
换底色就传 `--key-color`：

```bash
python3 tools/prep_pet.py --key-color "#00FF00"   # 绿底就用绿
```

水印删掉留下的透明空洞按 alpha 算范围，不会误判成宠物。

## 背景图（可选）

`pets/` 放一张 `background.png`（建议 240×320，像素风）就会被生成成 `pet_bg`，
画在标题牌和草地下面。 没有 `background.png` 就不生成、不占空间，页面用默认天空+草地。

## 替换宠物为小狗

0. 清理资源：
把 `main/pets/` 和 `pets/` 中宠物资源删除

1. 往 `pets/` 放图：
   ```
   waddle_1_r_moveforward.png   waddle_2_r_moveforward.png
   peck_1_r_idle.png            peck_2_r_idle.png
   fly_1_r_sprintup.png         fly_2_r_sprintup.png
   background.png               # 可选
   ```
2. 跑脚本（用项目里的 Python venv，已装 Pillow）：
   ```bash
   /Users/magic2014/.workbuddy/binaries/python/envs/default/bin/python tools/prep_pet.py
   ```
3. **新增了 `.c` 文件要重新扫描构建缓存**（CMake 用 `file(GLOB)` 收集源文件，缓存了旧列表）：
   ```bash
   IDF_PYTHON_ENV_PATH=/Users/magic2014/.espressif/python_env/idf5.5_py3.10_env \
     source /Users/magic2014/esp/v5.5.5/esp-idf/export.sh
   idf.py reconfigure
   idf.py build
   ```
   如果只是**覆盖**已有文件（不改数量），直接 `idf.py build` 即可。
4. 烧录：`idf.py flash monitor`

页面自动出现 `WADDLE / PECK / FLY` 三个动作，按键短按可逐个查看，`demo_pet.c` 一行都不用动。

## 命令速查

```bash
# 生成资源（默认读 pets/ → 写 main/pet/）
python3 tools/prep_pet.py
python3 tools/prep_pet.py --size 80          # 想要更大的宠物

# 进入 ESP-IDF 环境并构建
IDF_PYTHON_ENV_PATH=/Users/magic2014/.espressif/python_env/idf5.5_py3.10_env \
  source /Users/magic2014/esp/v5.5.5/esp-idf/export.sh
idf.py set-target esp32c3
idf.py reconfigure && idf.py build
idf.py flash monitor
```
