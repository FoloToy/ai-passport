<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# AI Passport 品牌视觉素材

本目录存放 AI Passport 的官方产品与品牌视觉参考图。用 AI 图像工具生成市场宣传图或展示效果图时，请以这些图片作为视觉基线。

## 概览

AI Passport 是一款穿戴设备（设备上与社区中名为 **Folotoy**）。参考图覆盖产品正面与背面，以及三种品牌色的正面外壳渲染图。每张图保持产品轮廓、屏显布局、接口与按键位置完全一致；EVA 变体仅改变外壳配色。

## 图片清单

### 产品参考图

| 文件 | 视角 | 说明 |
| --- | --- | --- |
| [`ai-passport-front.png`](ai-passport-front.png) | 正面 | 透明外壳；顶部 `22:02 / MON / 85%` 状态、`Folotoy` 标题、一个角色头像、`Token值 666 / 40000`、`GAME` 与 `IMAGE` 两个按键，以及标语 `The Open Wearable AI Passport`。 |
| [`ai-passport-back.webp`](ai-passport-back.webp) | 背面 | 透明外壳内可见 PCB；`FOLOTOY` logo、`AI PASS WEARABLE DEVICE`、电源/STA/BATT/USB/NFC 指示灯、`NFC` 标签、`AI PASSport` 标题与 `Wear it. Flash it. Make it anything.` 及二维码。 |

### 品牌色外壳渲染图（正面）

| 文件 | 配色 | 型号徽标 | 说明 |
| --- | --- | --- | --- |
| [`ai-passport-front-eva-01.png`](ai-passport-front-eva-01.png) | 紫色 | `01` | 初号机（Unit-01）配色；`TEST TYPE` 徽标。 |
| [`ai-passport-front-eva-00.png`](ai-passport-front-eva-00.png) | 橙色 | `00` | 零号机（Unit-00）配色；`PROTOTYPE MODEL` 徽标。 |
| [`ai-passport-front-eva-02.png`](ai-passport-front-eva-02.png) | 红色 | `02` | 二号机（Unit-02）配色；`PRODUCTION MODEL` 徽标。 |

三张配色渲染图均为 1024 × 1536 PNG，版式与标准正面图（`ai-passport-front.png`）一致：`22:02 / MON / 85%`、`TOKEN 666 / 40000`、`SYNC LEVEL` 进度条，以及两侧 `ENTRY` / `SYNC` 标签。

## 图片尺寸

| 文件 | 格式 | 尺寸 | 大小 |
| --- | --- | --- | --- |
| `ai-passport-front.png` | PNG | 605 × 931 | 460 KB |
| `ai-passport-back.webp` | WebP | — | 118 KB |
| `ai-passport-front-eva-01.png` | PNG | 1024 × 1536 | 2.0 MB |
| `ai-passport-front-eva-00.png` | PNG | 1024 × 1536 | 2.0 MB |
| `ai-passport-front-eva-02.png` | PNG | 1024 × 1536 | 2.0 MB |

## 如何生成新的渲染图

当基于这些参考图生成市场宣传或展示图时，请保持一致：

- **轮廓**：圆角卡片式穿戴设备，顶部有挂绳孔，两侧有侧键。
- **屏幕**：居中的单个矩形显示屏，显示时间/日期、标题、`Token值` 与两个操作按键。
- **Logo 与标签文字**：保留 `Folotoy` / `FOLOTOY`、型号名与标语。
- **配色**：以外壳参考图为基础，仅改变外壳/强调色以生成新变体（如白色、绿色或黑色版）。
- **比例**：正面视角按 2:3 竖版比例渲染效果最佳。

除非参考图本身允许复用，请把这些文件作为内部视觉基线，而不是原样搬运到公开发布的素材中。

## 使用方式

- 由于这些图通过会话挂载，用 AI 工具生成图片时请引用对应的 `cindy-media://` blob 或本地文件。
- 如果你生成了新的渲染图，请在上方表格中新增一行，并把生成文件链接进本目录。
- 保持产品事实（标签、型号名、状态文字）准确；不要虚构参考图中不存在的硬件特性。
