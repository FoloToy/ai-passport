<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 命运石之门0（Steins;Gate 0）阅读器

《命运石之门0》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 104 个章节入口、6 个结局、106 个鉴赏条目；解锁状态用 34 字节带版本 blob 记录

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

- 上游是从 Vela 工程转换好的场景快照，阅读器按场景表逐条执行。
- 全局解锁（章节/结局/鉴赏）只增不减，写失败时节流重试。
- 长 OK 在任意二级页都是返回，避免玩家卡在列表里。

## 分发

- 分支：`feature/port-steins0-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-steins0-landscape）
- 发布：`v1.0.0-steins0`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-steins0）
- 应用镜像 7,946,288 B / 合并镜像 8,011,824 B，占分区 **95.47%**，余量 **376,784 B**

## 上游

- 上游移植仓库：[xuanyide01/steins0-rw5](https://github.com/xuanyide01/steins0-rw5)；快照 dd19f6ce
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

