<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 9-nine- 天色天歌天籁音 阅读器

AI Passport 上的 320×240 横屏视觉小说阅读器，承载《9-nine- 天色天歌天籁音》
（Episode 2）的剧情、美术、音乐与完整 OP，全程离线，并在鉴赏室提供独立 OP 入口。

## 规模

- 56 个剧情节点、10,385 帧文本、21 章。
- 4 组选项 / 7 个选项、2 个结局记录。
- 1,102 张图像、22 组 / 72 个 CG 差分；OP 623 帧 @5fps（124.66 秒）。
- 音频为 12 kHz 单声道 IMA ADPCM，含标题曲与 OP 曲。

## 操作

| 按键 | 阅读中 | 列表 / 选项 |
| --- | --- | --- |
| 短按 OK / UP / DOWN | 推进；打字中先补全，长句按分页推进 | 移动 / 确认 |
| 长按 OK | 打开菜单 | 返回 |
| 长按 UP | 快进；选项与结局处停 | - |
| 长按 DOWN | 开关自动阅读 | - |

鉴赏室包含 CG、剧情回想、章节、音乐与 **OP 鉴赏**。OP 不需要通关即可观看；自然结束、
短按或长按都会回到同一行。鉴赏室浏览保持静音，返回标题页会恢复标题曲。

## 实现说明

- OP 从本地资源包播放 623 帧 @5fps（124.66 秒），不分配整帧解码缓冲。
- 游戏内菜单提供 3 个手动存档位、读取、历史、字号与分页设置。
- 阅读器沿用框架的页面 / 按键 / 持久化契约，只替换本作的内容与阅读层。

## 发布

- 分支：`feature/nine-sorairo-20261125`
  （<https://github.com/Shinku-Chen/ai-passport/tree/feature/nine-sorairo-20261125>）
- Release：`v1.0.0-nine-sorairo`
  （<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nine-sorairo>）
- 社区条目：<https://ai-passport.folotoy.cn/plays/9-nine-2>
- 固件：`FoloToy-AI-Passport-full.bin`，**7,266,952 B**（合并镜像）

## 上游

- 来源：[`xuanyide01/9-nine-rw5`](https://github.com/xuanyide01/9-nine-rw5)，
  固定提交 `6a8907a5ae576f4377abe4b74f15bd9af13efe07`。
- 框架：本 fork 的 `feature/galgame-framework`。
