<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 命运石之门（Steins;Gate）阅读器

《命运石之门》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 上游 steins-rw5 0.3.0 的全部剧本；分支内没有可核实的章/句计数，此处不写数字

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

- 正文固定 4 行排版，分页单位按字号换算，行首禁则标点留一格余量。
- 「关于」页正文用 1024 字节缓冲，128 字节会把 9 行介绍截断。
- 标题页只改两行标签颜色，不重跑整页渲染，避免每次按键重读 NVS 造成卡顿。

## 分发

- 分支：`feature/port-steins-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-steins-landscape）
- 发布：`v1.0.0-steins`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-steins）
- 应用镜像 7,975,280 B / 合并镜像 8,171,888 B，占分区 **97.35%**，余量 **216,720 B**

## 上游

- 上游移植仓库：[xuanyide01/steins-rw5](https://github.com/xuanyide01/steins-rw5)；快照 28e5b52f
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

