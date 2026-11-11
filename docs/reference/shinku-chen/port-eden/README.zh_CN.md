<p align="right"><a href="README.md">English</a> · <strong>简体中文</strong></p>

# eden*（eden*）阅读器

《eden*》的 AI Passport 横屏 320×240 视觉小说阅读器，属于 2026-11 的 `v1.0.0` 批次（十二部同批）。剧本、背景、立绘与事件 CG 全部打包进应用分区，离线可读。

## 规模

- 21 章、8,860 句对白、295 组 CG；打包器内部计数为 2,178 个画面（1,594 个补丁差分），但上游当前树里只有 855 张 JPEG、gallery 引用 2,341 次（去重 2,076 个 id）——该数字是打包器口径，不是文件数。中文字库 2,681 字形。

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

- 真机实测最大连续堆块只有 8,704 B，所以**不做分带**：复用一块 264×168 静态画布，用一个 320×240 的 lv_image 放大（scale 366，cover 1.43×）铺满。
- 图像对象要置底（move_to_index 0），否则 setParent 会盖住文字，表现为「一大块黑」。
- 任何大于 8 KB 的新堆分配都不可用；分带与整幅缩放两条路都已实测放弃。

## 分发

- 分支：`feature/port-eden-landscape`（https://github.com/Shinku-Chen/ai-passport/tree/feature/port-eden-landscape）
- 发布：`v1.0.0-eden`（https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-eden）
- 应用镜像 7,785,072 B / 合并镜像 7,850,608 B，占分区 **93.54%**，余量 **538,000 B**

## 上游

- 上游移植仓库：[xuanyide01/eden-rw5](https://github.com/xuanyide01/eden-rw5)
- 应用基线：[`FoloToy/ai-passport`](https://github.com/FoloToy/ai-passport) 的 `feature/galgame-framework`

