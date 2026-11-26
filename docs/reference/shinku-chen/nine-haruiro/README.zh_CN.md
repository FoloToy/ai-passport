<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 9-nine- 春色春恋春熙风 阅读器

AI Passport 上的 320×240 横屏视觉小说阅读器，承载《9-nine- 春色春恋春熙风》
（Episode 3）的非 R18 中文主线、两个结局、鉴赏室与 OP，全程离线。

## 规模

- 78 个剧情节点 / 15 章、10,659 帧对白。
- 13 个选项节点 / 13 个可见选项、2 个结局里程碑（branch 与 true）。
- 23 个状态变量（10 个永久 key）、2,368 个视觉快照 / 9,333 层、18 组 / 100 个
  CG 差分、2 首 BGM。
- OP：493 帧 @5fps（98.62 秒）。

## 操作

| 按键 | 阅读中 | 列表 / 选项 |
| --- | --- | --- |
| 短按 OK / UP / DOWN | 推进；打字中先补全，长句按分页推进 | 移动 / 确认 |
| 长按 OK | 打开菜单 | 返回 |
| 长按 UP | 快进；选项与结局处停 | - |
| 长按 DOWN | 开关自动阅读 | - |

## 实现说明

- 源快照不含 11 个 R18 场景与全部语音，本移植既不重建也不虚构这些内容；这里的
  “完整”是相对于该快照而言：每一帧对白、选项目标、两个结局与全部美术引用都在包内。
- 10,659 帧逐行对账，10,674 步轨迹与源 `engine.js` 完全一致。
- 阅读器保留框架的 16 px 三行 / 20 px 两行分页与三键交互契约。

## 发布

- 分支：`feature/nine-haruiro-20261125`
  （<https://github.com/Shinku-Chen/ai-passport/tree/feature/nine-haruiro-20261125>）
- Release：`v1.0.0-nine-haruiro`
  （<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nine-haruiro>）
- 社区条目：<https://ai-passport.folotoy.cn/plays/9-nine-3>
- 固件：`FoloToy-AI-Passport-full.bin`，**8,127,984 B**（合并镜像）

## 上游

- 来源：[`xuanyide01/9-nine-rw5`](https://github.com/xuanyide01/9-nine-rw5)，
  固定提交 `6a8907a5ae576f4377abe4b74f15bd9af13efe07`。
- 框架：本 fork 的 `feature/galgame-framework`。
