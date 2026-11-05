<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# 男娘恋爱物语（NianNiang Love Story）阅读器

《男娘恋爱物语》的 AI Passport 竖屏 240×320 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 13 个场景、1,248 句对白、2 处选项、4 个结局、16 张背景、15 张事件 CG、30 张表情立绘

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

- 本作是 240×320 竖屏（分支名仍带 -landscape，属历史命名）。
- 画面拆 4 条 240×80 的独立缓冲，每条带自己的 LVGL 描述符，整帧只刷一次；共用缓冲会导致三条带都显示最后写入的内容。
- 鉴赏室看 CG 用 contain（整幅可见），阅读中仍用 cover + 顶部对齐。
- 立绘底边贴屏幕底（320 − h），不被对话框切掉下半身。

## 分发

- 分支：`feature/port-nianniang-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-nianniang-landscape）
- 发布：`v1.0.0-nianniang`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nianniang）
- 应用镜像 1,748,528 B / 合并镜像 1,814,064 B，占分区 **21.01%**，余量 **6,574,544 B**

## 上游

- 上游移植仓库：[hrk666666/NianNiang-MiBand-10](https://github.com/hrk666666/NianNiang-MiBand-10)；其上游为 hrk666666/Senren-Banka-MiBand-10
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

