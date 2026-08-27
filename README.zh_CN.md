<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# FoloToy AI Passport — 项目

本仓库是 [`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 fork，
在 AI Passport 可穿戴设备（ESP32-C3、8 MB Flash、无 PSRAM）上开发了多个独立应用。
每个项目各自位于一个 `feature/*` 分支上，下面逐个介绍。本页是**该 fork 所承载各项目的目录**；
板卡基线、硬件与开发流程见上游 [`docs/README.md`](docs/README.md)。

## 项目

### 音效钥匙扣（Voice Keychain）

把 AI Passport 变成口袋音频播放器的音效钥匙扣。打开即可播放来自几十个角色包的数百条中文语音片段
——jojo、meme cat、刘华强、哈吉米、奶龙等等。三个按键驱动：**UP / DOWN** 在列表中移动，
**OK** 进入目录、选择片段或播放，**OK（长按）** 进入设置（电量与音量）或返回。顶部栏显示电量
百分比。

- 分支：[`feature/voice-keychain`](https://github.com/Shinku-Chen/ai-passport/tree/feature/voice-keychain)
- 档案：[`plays/shinku-chen/voice-keychain/`](plays/shinku-chen/voice-keychain/README.zh_CN.md)

### 今天吃啥（What to Eat Today）

一个食物转盘决策小助手。按住 **UP** 播放"今天午餐要吃什么呢？"引导动画，按住 **DOWN** 滚动食物
选择器，松开停在某个随机食物上。实时显示电量，空闲一段时间后自动关机。帧画面以索引色素材嵌入，
无需 PSRAM 缓冲即可运行。

- 分支：[`feature/cheerful-goodall`](https://github.com/Shinku-Chen/ai-passport/tree/feature/cheerful-goodall)
- 入口：`main/demo_eat_what.c`；封面素材 `assets/images/eat-what-cover.png`

### 生字卡片识记（Shengzi Cards）

一个汉字闪卡识记应用。三种模式：

- **浏览（Browse）** — 滚动浏览字卡。
- **自测（Self-test）** — 标记每个字认识/不认识。
- **拼读（Spell）** — 看拼音猜字。

长按 **UP / DOWN** 切换模式；**OK 短按**在自测/拼读模式里"揭晓"。已认识标记持久化到 NVS。

- 分支：[`feature/shengzi-cards`](https://github.com/Shinku-Chen/ai-passport/tree/feature/shengzi-cards)
- 入口：`main/demo_shengzi.c`

## 说明

- 每个应用都是基于上游基线的一个独立 `feature/*` 分支。
- 硬件基线、BSP 与贡献规则来自上游仓库（`docs/README.md`、`AGENTS.md`、`docs/contribution/`）。
- 这些发布中沉淀的可复用工程经验位于 [`docs/experiences/shinku-chen/`](docs/experiences/shinku-chen/)（上游）。
