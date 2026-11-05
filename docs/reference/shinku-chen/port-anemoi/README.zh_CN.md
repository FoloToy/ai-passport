<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# anemoi（anemoi）阅读器

《anemoi》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 71/264 场景（共通篇 23 章 + 委托信/任务），正文 764,847 / 3,088,939 字节；30 个手动存档 + 自动档

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

- 背景与事件 CG 为 320×180 原始规格，按 320×240 面板等比铺满。
- 立绘分「身体 + 原生表情」两层合成，表情不重复存整张图。
- 委托板与章节列表是本作自己的页面，不套用共通阅读器菜单。
- 本版只覆盖共通篇与委托内容，走到路线正篇会显示分卷边界页而不是静默跳章。

## 分发

- 分支：`feature/port-anemoi-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-anemoi-landscape）
- 发布：`v1.0.0-anemoi`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-anemoi）
- 应用镜像 8,240,848 B / 合并镜像 8,306,384 B，占分区 **99.01%**，余量 **82,224 B**

## 上游

- 上游移植仓库：[xuanyide01/anemoi-rw5](https://github.com/xuanyide01/anemoi-rw5)；快照 3e27714c
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

