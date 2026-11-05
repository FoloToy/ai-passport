<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# PARQUET（PARQUET）阅读器

《PARQUET》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 107 个场景 / 434 个剧本块 / 11,205 条 op / 11,098 句正文、4 个选项、4 个结局；367 张 CG（37 组）与 979 个立绘 id 是打包器的**引用计数**（`images/p014`、`s015` 目录里实际是 400 张 JPEG）。

## 三键操作

| 按键 | 阅读中 | 列表 / 选项 |
| --- | --- | --- |
| OK 短按 | 推进一页；本页还在打字时立刻显示整页 | 确认 |
| OK 长按 | 打开菜单 | 返回上一级 |
| UP 短按 | 推进一页 | 光标上移 |
| UP 长按 | 快进，松手即停 | — |
| DOWN 短按 | 历史回看 | 光标下移 |
| DOWN 长按 | 开关自动阅读 | — |

## 实现要点

- 美术用双带画布（2 × 320×120），一帧先画两条带再一次 flush，合成全屏 CG 时不再整幅缩放。
- 章节自动续播：同路线才自动接下一章，末章或跨路线进结局页；转章立刻写自动档。
- 立绘基线由解码高度回推（body_dy + h 恒为 150），不再出现悬空缝隙。
- 快进是整页瞬显（叠加去重画面指纹，画面未变不重解码）。

## 分发

- 分支：`feature/port-parquet-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-parquet-landscape）
- 发布：`v1.0.0-parquet`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-parquet）
- 应用镜像 7,181,616 B / 合并镜像 7,247,152 B，占分区 **86.29%**，余量 **1,141,456 B**

## 上游

- 上游移植仓库：[xuanyide01/parquet-rw5](https://github.com/xuanyide01/parquet-rw5)；快照 8ab38afe
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

