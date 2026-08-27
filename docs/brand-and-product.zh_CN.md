<p align="right">
  <strong>简体中文</strong> · <a href="brand-and-product.md">English</a>
</p>

# 品牌与产品说明（Brand & Product Info）

> **面向读者**：用户、开发者、AI agent 以及任何想了解 AI Passport 品牌与产品的人
> **读取时机**：首次接触本产品、需要了解品牌定位 / 产品名称 / 官方入口 / 产品规格，或准备对外引用产品信息之前
> **关联文档**：设备硬件规格见 [hardware-design/specifications.md](hardware-design/specifications.md)；凭证与隐私入仓红线见 [contribution/doc-conventions.md](contribution/doc-conventions.md)。

---

## 1. 品牌与产品

- **产品名**：**AI Passport**（英文全称 *FoloToy AI Passport*，官网英文名 "AI Passport | Open Wearable AI Agent"）。
  - 名称统一写 `AI Passport`，不翻译、不加后缀、不派生变体；中文语境同样用「AI Passport」，不另造译名。
- **品牌主体**：**FoloToy**（"We are creating AI toys"，创造 AI 玩具）。
  - GitHub 组织：`github.com/FoloToy`
- **产品开源仓库**：`github.com/FoloToy/ai-passport`——AI Passport 的开源**开发基线**，集中存放已确认的硬件事实、稳定接口、资源边界、参考实现与验收方法。

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

## 3. 官方入口

| 入口 | 地址 |
| --- | --- |
| 产品官网（中文） | `https://ai-passport.folotoy.cn/` |
| 产品官网（英文） | `https://ai-passport.folotoy.cn/en/` |
| 使用指南 | `https://ai-passport.folotoy.cn/guides/` |
| 快速上手 | `https://ai-passport.folotoy.cn/guides/getting-started/` |
| 官方玩法库 | `https://ai-passport.folotoy.cn/plays/`（主页锚点 `/#official-plays`） |
| 浏览器刷机工具 | `https://ai-passport.folotoy.cn/tools/web-flasher/`（产品官网内置入口） |
| **FOLOTOY 通用刷机工具** | `https://tool.folotoy.cn/`（独立站点，浏览器 WebSerial 刷机 + 实时设备日志，固件不上传服务器） |
| **官方配置小程序** | 在小程序搜索框搜索 **「FoloToy AI Passport」** 即可找到 |

### 官方玩法库

当前官网展示 13 款官方玩法（编号为玩法 ID，非排序）：模块拓展「AI 像素宠物」（060）、多人互动「口袋剪刀石头布」（025）、创意游戏「口袋番茄钟」（044）、创意游戏「答案之书」（008）、日常陪伴「Claude Buddy」（016）、效率工具「口袋秒表」（046）、效率工具「单词熊」（048）、创意游戏「口袋俄罗斯方块」（050）、创意游戏「老板宠物」（061）、多人互动「通行证雷达」（062）、创意游戏「Bad Apple 播放器」（063）、日常陪伴「今天心情怎么样」（064）、创意游戏「牛来互动播放器」（065）。

> ⚠️ 玩法列表以官网为准，可能会和实际有差距，以实际为准。

### 浏览器刷机工具（两处入口）

FoloToy 提供两处浏览器刷机入口，均可安全写入本地固件，固件文件不上传服务器：

- **`https://ai-passport.folotoy.cn/tools/web-flasher/`**：AI Passport 产品官网内置的刷机入口，与产品页 / 玩法安装流程集成。
- **`https://tool.folotoy.cn/`**：**FOLOTOY 通用刷机工具**（独立站点）——适用于 FoloToy 设备线的通用浏览器刷机工具，无需安装软件，通过 WebSerial 写入本地固件并查看实时设备日志。

引用"在线刷机 / 浏览器刷机"入口时，产品官网场景优先用官网内置入口；需要给用户通用刷机工具时用 `tool.folotoy.cn`。

### 官方配置小程序

使用官方配置小程序前，需先通过扫描设备**背后的二维码**刷入原始固件。刷好后，在小程序搜索框里搜索
**「FoloToy AI Passport」** 即可找到官方配置入口。在官方小程序里可设置**头像**、**姓名**，以及播放的
**音乐**、设备屏幕上显示的**文字**。

## 3.1 品牌视觉素材

官方产品与品牌视觉参考图存于 [`docs/assets/brand/`](assets/brand/README.md)（索引见同目录 `README.md` / `README.zh_CN.md`）。包含产品**正面**与**背面**实拍图，以及三种品牌配色外壳的正面渲染图（EVA 紫 / 橙 / 红三款）。

这些图是 AI Passport 市场宣传与展示物料的**视觉基线**。可用 AI 图像工具基于这些参考图生成效果图：把参考图作为整张图的基底，保留其外壳、按键、接口、挂绳孔、Logo 文字与配色原样，只把参考图的屏幕区域**重绘**成用户要求的内容。参考图的屏幕区域只是占位——屏幕的尺寸、比例、圆角与外壳内位置要与参考完全一致，而不是重新塑形或移动。生成设备图时必须传一张参考图作为输入，不能只靠文字描述。新生成的渲染图请登记进 [`assets/brand/README.md`](assets/brand/README.md) 索引。素材作为内部视觉基线使用，除非参考图本身允许复用，否则不要原样搬运到公开发布物料中。

## 4. 产品规格

设备产品规格见 [hardware-design/specifications.md](hardware-design/specifications.md)（尺寸、重量、电池、充电、NFC、按键等）。

## 5. 开源与授权

- 产品开源仓库：`github.com/FoloToy/ai-passport`。
- 协议：**MIT License**（Copyright (c) 2026 FoloToy）。
- 仓库常被 fork 后二次开发；fork 约定见仓库 `docs/fork-guide.md`。

## 6. 命名与多语言

- 名称 `AI Passport` 在官网中 / 英双版保持一致，不翻译。
- 官网提供中文（`/`）与英文（`/en/`）两版。
- 本仓库 README 提供中英双版（`docs/README.md` / `docs/README.zh_CN.md`）；本文档若需增加英文版，按仓库文档语言约定处理。

## 7. 与工程事实源的关系

- 本文档是**品牌与产品层**的说明，承载对外展示口径（名称、定位、官方入口、规格引用）。
- 设备产品规格见 [hardware-design/specifications.md](hardware-design/specifications.md)。
- 硬件引脚、资源边界等工程事实以仓库 `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md` 与 `components/bsp/include/bsp_pins.h` 为准，本文档不替代工程文档。
- 网站入口以官网实际页面为准；如官网结构变化，优先更新官网并回同步本文档。
