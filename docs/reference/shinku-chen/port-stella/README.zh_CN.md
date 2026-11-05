<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 星之终途（Stella of the End）阅读器

《星之终途》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。**正篇 21 章 + 后日谈 9 段**（合计 10,975 句）的剧本、背景、立绘与事件 CG 打包进应用分区，离线可读。

> 后日谈入口：标题页的「后日谈」项，需先通关正篇解锁；从 `story/101` 开始。打包器已收录全部 30 个场景（1–21 与 101–109）。

## 规模

- 21 章、9,425 句对白、1 个结局（第 21 章）

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

- 本批最紧的分区：只剩 66,992 B（99.20%），改任何内容前必须先算 Flash 预算。
- 说话人名字用 LV_LABEL_LONG_MODE_DOTS：LVGL 9 的 LV_LABEL_LONG_DOT 是「多行 + 省略」，会折行；CLIP + LV_SIZE_CONTENT 会把宽度算成 0。
- 列表高亮只给行标签着色（固定宽 304），不再同时高亮数值列，中间缝隙形成的竖线消失。
- 面板压到 156..240（84 px），正文贴顶，页码移出面板。

## 分发

- 分支：`feature/port-stella-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-stella-landscape）
- 发布：`v1.0.0-stella`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-stella）
- 应用镜像 8,256,080 B / 合并镜像 8,321,616 B，占分区 **99.20%**，余量 **66,992 B**

## 上游

- 上游移植仓库：[xuanyide01/stella-rw5](https://github.com/xuanyide01/stella-rw5)；快照 4fa1e7b5
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

