<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 爱上火车·二六线 阅读器

AI Passport 上的 320×240 横屏视觉小说阅读器，承载《爱上火车》二六线的四章内容、
35 张 CG 与完整 283.88 秒标题曲，全程离线。

## 规模

- 4 章、1,537 段、675 个场景。
- 6 组 35 张 CG；本线为线性剧情，只有一个结局。
- 19 屏 GalGame 框架：标题、对话、菜单、3 个手动存档位、自动进度、历史、设置，
  以及 CG / 回想 / 章节 / 音乐鉴赏室。

## 操作

| 按键 | 阅读中 | 列表 / 菜单 |
| --- | --- | --- |
| 短按 OK / UP / DOWN | 推进；打字中先补全，长句按分页推进 | 移动 / 确认 |
| 长按 OK | 打开菜单 | 返回 |
| 长按 UP | 立即跳到下一句；按住持续，松手停 | - |
| 长按 DOWN | 开关自动阅读 | - |

## 实现说明

- 场景保留源构图：背景 + 打包期已合成的立绘 / 表情补丁，按精确 16:9 的 320×180
  显示，不裁切。
- CG 鉴赏分页显示全部 35 张，包括超过旧框架 32 位 CG 上限的第 33–35 张。
- 标题曲为原作完整 283.88 秒、4 kHz 单声道源的 4 位 IMA ADPCM（586 KB），播放时
  线性插值到 16 kHz。
- 章节鉴赏与 CG 回想都是试读，不会授予永久结局；只有「开始游戏」正式读完四章才
  记录通关。

## 发布

- 分支：`feature/maitetsu-20261125`
  （<https://github.com/Shinku-Chen/ai-passport/tree/feature/maitetsu-20261125>）
- Release：`v1.0.0-maitetsu`
  （<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-maitetsu>）
- 社区条目：<https://ai-passport.folotoy.cn/plays/community-386921bf>
- 固件：`FoloToy-AI-Passport-full.bin`，**7,825,488 B**，分区占用 **94.02%**，
  剩余 **497,584 B**

## 上游

- 来源：[`xuanyide01/maitetsu-rw5`](https://github.com/xuanyide01/maitetsu-rw5)，
  固定提交 `9c008406853affe973ceb80f62128696054365e7`。
- 框架：本 fork 的 `feature/galgame-framework`。
