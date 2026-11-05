<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# CLANNAD（CLANNAD）阅读器

《CLANNAD》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 212 个场景 / 195 章 / 150,288 条指令、19 条路线，174 个鉴赏条目

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

- 对话框贴底（170..240），正文贴面板顶，页码移到面板外，避免压住正文。
- 章节条用 LV_SIZE_CONTENT 量宽，长章名不再被裁成固定宽。
- 立绘基线由解码后的实际高度回推，并按说话人切换表情。
- 分区只剩 173,056 B，任何内容改动前先做 Flash 预算。

## 分发

- 分支：`feature/port-clannad-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-clannad-landscape）
- 发布：`v1.0.0-clannad`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-clannad）
- 应用镜像 8,150,016 B / 合并镜像 8,215,552 B，占分区 **97.92%**，余量 **173,056 B**

## 上游

- 上游移植仓库：[xuanyide01/clannad-rw5](https://github.com/xuanyide01/clannad-rw5)；快照 2c677208
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

