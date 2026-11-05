<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 纸上的魔法使（Paper no Mahou Tsukai）阅读器

《纸上的魔法使》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 47 个场景脚本 / 56,634 条指令 / 44,521 句对白、4 处选项、6 个结局、80 组 415 张事件 CG、341 张立绘（163 整块 + 178 个表情差分补丁）

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

- tjpgd 输出是 B, G, R，直接拼 RGB565 会红蓝互换，画面红色越多越发紫；写画布前统一做 bgr→rgb565 换序。
- CG 铺满的缩放比在运行时按画布实际高度算（max(320/w, 240/h)），不写死常数。
- 面板 152..240（88 px），正文贴面板顶，页码在面板外。

## 分发

- 分支：`feature/port-paper-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-paper-landscape）
- 发布：`v1.0.0-paper`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-paper）
- 应用镜像 6,289,312 B / 合并镜像 6,354,848 B，占分区 **75.56%**，余量 **2,033,760 B**

## 上游

- 上游移植仓库：[xuanyide01/paper-rw5](https://github.com/xuanyide01/paper-rw5)
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

