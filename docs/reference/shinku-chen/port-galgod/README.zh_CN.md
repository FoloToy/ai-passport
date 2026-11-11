<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 想成为Galgame领域大神！！！（Galgame Master）阅读器

《想成为Galgame领域大神！！！》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 18 章 / 5,457 句 / 119,410 字、15 组选项（含答题计分与 bad_end_flag 分支）、4 个结局与后日谈、55 张立绘、38 张背景

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

- 自带阅读/UI 层（不复用共享 gal 框架）。
- 下半条画布接缝：给条带画布设 pivot(0,0)，按中心缩放会偏 1–3 px。
- 同帧立绘去重按内容（JPEG 指针 + 长度）而不是 id，包里同一角色多个槽位指向同一份图。
- 面板高度压到 88 px，正文贴面板顶，页码移到面板外。

## 分发

- 分支：`feature/port-galgod-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-galgod-landscape）
- 发布：`v1.0.0-galgod`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-galgod）
- 应用镜像 3,285,824 B / 合并镜像 3,351,360 B，占分区 **39.48%**，余量 **5,037,248 B**

## 上游

- 上游移植仓库：[hrk666666/GalGod-MiBand-10](https://github.com/hrk666666/GalGod-MiBand-10)；快照 ebe085d2；美术来自 mcpotato1123/galgod-band v2.0.0
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

