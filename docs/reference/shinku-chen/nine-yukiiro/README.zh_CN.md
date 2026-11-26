<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 9-nine- 雪色雪花雪之痕 阅读器

AI Passport 上的 320×240 横屏视觉小说阅读器，承载《9-nine- 雪色雪花雪之痕》
（Episode 4）的完整剧情、全部分支与结局记录、鉴赏室与 OP，全程离线。

## 规模

- 212 个剧情节点、12,665 帧文本、23 章。
- 47 个选项节点 / 70 条记录（源闸门下 63 条可达，7 条结构上不可达）。
- 2 个结局记录（`loop1` 中途完成、`true` 终局）、19 组 / 108 个 CG 差分、
  78 个运行时变量。
- 668,498 字节 UTF-8 文本，分 84 个 zlib 块。

## 操作

| 按键 | 阅读中 | 列表 / 选项 |
| --- | --- | --- |
| 短按 OK / UP / DOWN | 推进到下一个完整页或下一句；打字中与自动播放中也立即生效 | 移动 / 确认 |
| 长按 OK | 打开菜单 | 返回 |
| 长按 UP | 跳到下一整句；每拍最多一句，松手停 | - |
| 长按 DOWN | 开关自动播放 | - |

## 实现说明

- 美术与剧情分块来自上游手环工程的处理图；未提交的 PC 原作脚本不重建。PC 语音与
  成人素材不包含在内（手环源本身已是非成人、零语音）。
- 章节预览与鉴赏室播放不会写入自动进度或永久进度；长按松手不会额外触发一次短按。
- 320×180 舞台保留源 16:9 构图，下方是对话框，并沿用框架的三键分页契约。

## 发布

- 分支：`feature/nine-yukiiro-20261125`
  （<https://github.com/Shinku-Chen/ai-passport/tree/feature/nine-yukiiro-20261125>）
- Release：`v1.0.0-nine-yukiiro`
  （<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nine-yukiiro>）
- 社区条目：<https://ai-passport.folotoy.cn/plays/9-nine-4>
- 固件：`FoloToy-AI-Passport-full.bin`，**8,258,768 B**（合并镜像）

## 上游

- 来源：[`xuanyide01/9-nine-rw5`](https://github.com/xuanyide01/9-nine-rw5)，
  固定提交 `6a8907a5ae576f4377abe4b74f15bd9af13efe07`。
- 框架：本 fork 的 `feature/galgame-framework`。
