<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 魔法少女的魔女审判（Mahou Shojo no Majo Saiban）阅读器

《魔法少女的魔女审判》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 458 个源剧本文件 / 424 个场景 / 90,243 条指令，559 张画面（62 背景、172 CG、151 立绘等）

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

- 包内不是对白流而是源引擎指令：由 C 虚拟机按 engine.js / scene.js 语义执行，支持 call/return、offer/choice、审判伤害与手册版本。
- 本作自带阅读/UI 层（不复用共享 gal 框架），页面包含魔女手册与结局页。
- 30 个手动存档 + 独立自动档，写入失败不改旧档。

## 分发

- 分支：`feature/port-manosaba-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-manosaba-landscape）
- 发布：`v1.0.0-manosaba`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-manosaba）
- 应用镜像 7,763,280 B / 合并镜像 7,959,888 B，占分区 **94.77%**，余量 **428,720 B**

## 上游

- 上游移植仓库：[xuanyide01/manosaba-rw5](https://github.com/xuanyide01/manosaba-rw5)；快照 a309a361
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

