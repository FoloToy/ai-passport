# 仓库说明（Repo Info）

> **面向读者**：AI agent、开发者、fork 用户以及任何想了解本仓库背景的人
> **读取时机**：首次接触本仓库、需要了解产品名称 / 官网入口 / 硬件规格 / 仓库协议，或准备引用官方链接之前

---

## 1. 产品与仓库

- **产品名**：**AI Passport**（英文全称 *FoloToy AI Passport*，官网英文名 "AI Passport | Open Wearable AI Agent"）。
  - 名称统一写 `AI Passport`，不翻译、不加后缀、不派生变体；中文语境同样用「AI Passport」，不另造译名。
- **开发组织**：**FoloToy**。
  - GitHub 组织：`github.com/FoloToy`
  - 组织定位："We are creating AI toys"（创造 AI 玩具）
- **本仓库**：`github.com/FoloToy/ai-passport`，是 AI Passport 的开源**开发基线**——集中存放已确认的硬件事实、稳定接口、资源边界、参考实现与验收方法，供 AI agent 与开发者实现并构建应用。

## 2. 产品定位

AI Passport 是一个**开放式可穿戴 AI 智能体**：

> 可佩戴、可安装玩法、可由你重新定义的开放式 AI 智能体。
> *A wearable, open AI agent you can redefine with installable plays.*

- 英文表述：**Open Wearable AI Agent**
- 中文 slogan：**「一张与 AI Agent 一起开造世界的通行证」**
- 辅助表述：**「简单开放 人人可造」**（"Open & simple, made by anyone"）
- 核心口号：**WEAR · PLAY · CREATE**（佩戴 / 游玩 / 创造）

### 三种用法

| 用法 | 说明 |
| --- | --- |
| **WEAR（佩戴）** | 出厂即身份卡。经本地 BLE 同步名称、头像、自我介绍与全屏图片，成为"开造世界的 AI 通行证"；默认玩法内置一款趣味小游戏 |
| **PLAY（游玩）** | 从官方玩法库选择玩法，在浏览器一键刷入；身份卡功能保留，只替换游戏内容 |
| **CREATE（创造）** | 会写代码就自己做固件；可配合 AI Agent（Codex / Claude Code / TRAE 等）从一句需求开始开发玩法 |

## 3. 硬件规格

以下为设备硬件规格（源自官方产品页）：

| 项目 | 规格 |
| --- | --- |
| 形态 | 可穿戴，透明外壳（屏幕 / 主板 / NFC / 麦克风 / 扬声器 / 电池均为产品语言一部分） |
| 尺寸 | 60 × 95 × 8.5 mm |
| 重量 | 50 g |
| MCU | ESP32-C3（8MB Flash） |
| 显示 | 240 × 320 彩色 TFT |
| 无线 | 2.4 GHz Wi-Fi（802.11 b/g/n）；Bluetooth® 5 LE（可同步头像 / 昵称 / Token / 个性化内容） |
| NFC | 被动 NFC 标签（NTAG213，支持读卡器 / 手机读写普通 NDEF 数据） |
| 输入 | 上 / 下 / 确定三枚功能键 + 独立电源键（硬件实现，不可改功能） |
| 电源 | 按住电源键 0.5s 开机；长按约 2s 关机；自动息屏后按任意功能键唤醒 |
| 音频 | 内置麦克风 + 内置扬声器 |
| 充电 | USB Type-C 2.0 5V |
| 电池 | 内置 520 mAh 可充电锂电池 |
| 其他 | 专属二维码（QR fallback，含恢复固件入口） |

## 4. 官方入口

| 入口 | 地址 |
| --- | --- |
| 产品官网（中文） | `https://ai-passport.folotoy.cn/` |
| 产品官网（英文） | `https://ai-passport.folotoy.cn/en/` |
| 使用指南 | `https://ai-passport.folotoy.cn/guides/` |
| 快速上手 | `https://ai-passport.folotoy.cn/guides/getting-started/` |
| 官方玩法库 | `https://ai-passport.folotoy.cn/plays/`（主页锚点 `/#official-plays`） |
| 浏览器刷机工具 | `https://ai-passport.folotoy.cn/tools/web-flasher/`（产品官网内置入口） |
| **FOLOTOY 通用刷机工具** | `https://tool.folotoy.cn/`（独立站点，浏览器 WebSerial 刷机 + 实时设备日志，固件不上传服务器） |

### 官方玩法库

当前官网展示 13 款官方玩法（编号为玩法 ID，非排序）：模块拓展「AI 像素宠物」（060）、多人互动「口袋剪刀石头布」（025）、创意游戏「口袋番茄钟」（044）、创意游戏「答案之书」（008）、日常陪伴「Claude Buddy」（016）、效率工具「口袋秒表」（046）、效率工具「单词熊」（048）、创意游戏「口袋俄罗斯方块」（050）、创意游戏「老板宠物」（061）、多人互动「通行证雷达」（062）、创意游戏「Bad Apple 播放器」（063）、日常陪伴「今天心情怎么样」（064）、创意游戏「牛来互动播放器」（065）。

> ⚠️ 玩法列表以官网为准，可能会和实际有差距，以实际为准。

### 浏览器刷机工具（两处入口）

FoloToy 提供两处浏览器刷机入口，均可安全写入本地固件，固件文件不上传服务器：

- **`https://ai-passport.folotoy.cn/tools/web-flasher/`**：AI Passport 产品官网内置的刷机入口，与产品页 / 玩法安装流程集成。
- **`https://tool.folotoy.cn/`**：**FOLOTOY 通用刷机工具**（独立站点）——适用于 FoloToy 设备线的通用浏览器刷机工具，无需安装软件，通过 WebSerial 写入本地固件并查看实时设备日志。

引用"在线刷机 / 浏览器刷机"入口时，产品官网场景优先用官网内置入口；需要给用户通用刷机工具时用 `tool.folotoy.cn`。

## 5. 仓库与开源

- 开源仓库：`github.com/FoloToy/ai-passport`
- 协议：**MIT License**（Copyright (c) 2026 FoloToy）
- 仓库常被 fork 后二次开发；fork 约定见仓库 `docs/fork-guide.md`。

## 6. 命名与多语言

- 名称 `AI Passport` 在官网中 / 英双版保持一致，不翻译。
- 官网提供中文（`/`）与英文（`/en/`）两版。
- 本仓库 README 提供中英双版（`README.md` / `README.zh_CN.md`）；本说明若落仓，建议同样提供 `.zh_CN.md` 或按仓库文档语言约定处理。

## 7. 与工程事实源的关系

- 本文档是**仓库级**的元信息说明，承载对外展示口径（名称、定位、官网、规格、协议）。
- 硬件引脚、资源边界等工程事实以仓库 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md` 与 `components/bsp/include/bsp_pins.h` 为准，本文档不替代工程文档。
- 网站入口以官网实际页面为准；如官网结构变化，优先更新官网并回同步本文档。

## 8. 隐私与安全门限

- **设备二维码链接含隐私参数，禁止写入代码 / 仓库**：设备（卡片）背面二维码扫码后得到的链接带有隐私令牌参数，例如
  `https://ai-passport.folotoy.cn/trae/?s=4c11aeAAAAAA&k=CCAXAAAAAA`
  （`s`、`k` 为设备绑定的敏感标识，可能关联到具体设备或持有者）。此类链接**绝不允许**写进本仓库代码、配置、文档、示例、测试或 commit message。
- 处理此类链接时：不要在代码库中存放、打印、提交或复制进示例；仅可出现在用户本人可直接查看的介质（如设备贴纸 / 本地记录）中。
- 发现误提交应立即移除并告知维护者；仅删除工作区文件不能保证 Git 历史中的参数已失效。
- 对外文档需要演示该链接形态时，使用脱敏占位（如 `https://ai-passport.folotoy.cn/trae/?s=<secret>&k=<key>`），不得携带真实参数。
