<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# Summer Pockets REFLECTION BLUE（Summer Pockets REFLECTION BLUE）阅读器

《Summer Pockets REFLECTION BLUE》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 422 个场景、81,040 句正文、188 组选项、27 个地图阶段、14 条路线 / 252 个章节入口、136 组 CG（712 张差分）、292 张立绘与表情

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

- 对话框压到 176..240（64 px 高），正文 3 行，页码与提示移到面板外，换取更多画面面积。
- 说话人名字按内容量宽，不再固定宽度。
- 调暗阈值 90 s（其余作品 60 s），因为前几章单页停留时间偏长。

## 分发

- 分支：`feature/port-summer-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-summer-landscape）
- 发布：`v1.0.0-summer`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-summer）
- 应用镜像 7,585,456 B / 合并镜像 7,716,528 B，占分区 **91.86%**，余量 **672,080 B**

## 上游

- 上游移植仓库：[xuanyide01/summer-rw5](https://github.com/xuanyide01/summer-rw5)；快照 28f8a41c，由小米 Vela 工程转换
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

