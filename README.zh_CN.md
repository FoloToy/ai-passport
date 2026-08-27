<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# FoloToy AI Passport — 项目

本仓库是 [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 fork，
在 AI Passport 可穿戴设备（ESP32-C3、8 MB Flash、无 PSRAM）上开发了多个独立应用。每个项目各自位于
一个 `feature/*` 分支上，本页呈现**每个项目的完整内容**——它做什么、怎么用——让这套作品能从仓库
落地页被检索到。板卡基线、硬件、BSP 与贡献规则来自上游仓库。

## 项目

### 音效钥匙扣（Voice Keychain）

一个把 AI Passport 变成口袋音频播放器的音效钥匙扣。打开即可播放来自几十个角色包的数百条中文语音片段
——jojo、meme cat、刘华强、哈吉米、奶龙、小明剪膜等等。

**功能**

- **角色目录**：以可滚动列表浏览所有角色包；每个条目是一个语音包（如 jojo、MC、meme cat、刘华强、
  刘海柱、卡丘美雪、路银、印度阿三、吉一卡哇伊、哈吉米、奶龙、抱抱嘟大磊磊、小团团、小明剪膜）。
- **片段列表**：进入某个包查看其中的片段名。
- **一键播放**：按 OK 播放选中的片段；内置解码播放 8 kHz 单声道 IMA-ADPCM 音频。
- **设置**（长按 OK）：显示当前电量百分比与电压，调节播放音量。

**交互**

三个按键驱动整个应用。顶部栏显示标题，主界面显示电量百分比（如 `97%`）。

- **UP / DOWN** —— 移动选中项（长按滚动）。
- **OK** —— 进入目录 / 选择片段 / 播放。
- **OK（长按）** —— 打开设置，或返回。

长条目横向滚动以便看清全名；选中行高亮为蓝色。语音片段存放在挂载于 `/voices` 的 `voicefs` SPIFFS
数据分区，由 `tools/encode_voice.py` 生成（解码 → 重采样到 8 kHz 单声道 → IMA-ADPCM 4bit →
`main/voice_index.h` + `voicefs.img`），应用分别烧录合并固件镜像与该数据分区。

- 分支：[`feature/voice-keychain`](https://github.com/Shinku-Chen/ai-passport/tree/feature/voice-keychain)
- 档案：[`plays/shinku-chen/voice-keychain/`](plays/shinku-chen/voice-keychain/README.zh_CN.md)

### 今天吃啥（What to Eat Today）

一个食物转盘决策小助手。不知道吃什么？按住按键滚动食物彩票，松开停在某个随机食物上。

**交互**

三个按键驱动应用（按键回调跑在 button 任务里、保持非阻塞；动画循环用 LVGL timer 跑）：

- **按住 UP** —— 播放引导动画（"今天午餐要吃什么呢？"）。
- **按住 DOWN** —— 滚动食物选择器动画。
- **松开** —— 停在当前帧（落在的食物）。
- **OK（长按）** —— 返回菜单（由 `main.c` 统一拦截）。

"松开"靠轮询 `bsp_button_read_mv()` 检测（本 BSP 无 RELEASE 事件）：松开 ≈ 3300 mV，按住 < 2000 mV。

**功能**

- 右上角电量指示（读取 `bsp_battery_soc()`）实时显示电量；读值为 `-1`（不可用）时优雅降级。
- 空闲一段时间后自动关机。
- 无需 PSRAM：食物帧以 LVGL `LV_COLOR_FORMAT_I8` 索引色二进制嵌入（CMake `EMBED_FILES`），动画直接
  从 Flash 播放，无需帧缓冲。

帧画面来自 `main/eat_what_g1.bin` / `main/eat_what_g2.bin`（256 色调色板 + 每帧像素索引，由
`tools/generate_eat_what_assets.py` 生成）。封面素材 `assets/images/eat-what-cover.png`
（2048 × 2048）："展示汉堡、拉面、饺子、火锅、奶茶的可爱卡通电视"。

- 分支：[`feature/cheerful-goodall`](https://github.com/Shinku-Chen/ai-passport/tree/feature/cheerful-goodall)
- 入口：`main/demo_eat_what.c`

### 生字卡片识记（Shengzi Cards）

一个汉字闪卡识记应用。固件启动后直接进入该应用，使用 `ui_pixel` 主题（天空底色、草地、标题牌、吉祥物、
墨色描边面板）在 240 × 320 屏幕上呈现。

**三种模式**，长按 **UP / DOWN** 切换：

- **浏览（Browse）** —— 滚动浏览字卡（一张大字卡，上方拼音、下方一行提示）。
- **自测（Self-test）** —— 标记每个字认识/不认识。
- **拼读（Spell）** —— 看拼音猜字。

**OK 短按** 在自测/拼读模式里"揭晓"答案。"已认识"标记持久化到 NVS（`sz_data`），重启后进度不丢。
屏幕布局：标题栏（≈ y0–46）、拼音行（y ≈ 58）、大字卡片区（y ≈ 78–218）、底部信息行（y ≈ 255–290）。

- 分支：[`feature/shengzi-cards`](https://github.com/Shinku-Chen/ai-passport/tree/feature/shengzi-cards)
- 入口：`main/demo_shengzi.c`

## 说明

- 每个应用都是基于上游基线的一个独立 `feature/*` 分支。
- 硬件基线、BSP 与贡献规则来自上游仓库（`docs/README.md`、`AGENTS.md`、`docs/contribution/`）。
- 这些发布中沉淀的可复用工程经验位于 [`docs/experiences/shinku-chen/`](docs/experiences/shinku-chen/)（上游）。
