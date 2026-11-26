<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 9-nine- 九次九日九重色 阅读器

AI Passport 上的 320×240 横屏视觉小说阅读器，承载《9-nine- 九次九日九重色》
（Episode 1）的完整剧情、表情差分、CG、标题主视觉与两首 BGM，全程离线。

## 规模

- 17 章、74 个剧情节点、8,049 帧对白。
- 5 组选项 / 9 个选项；2 个结局（一周目 loop end、二周目 true end）。
- 15 个状态变量（含 5 个永久 key）；22 组 / 73 个 CG 差分，17 个章节已读标记。

## 操作

| 按键 | 阅读中 | 列表 / 选项 |
| --- | --- | --- |
| 短按 OK / UP / DOWN | 立即推进；长句按分页单元推进 | 移动 / 确认 |
| 长按 OK | 打开菜单 | 返回 |
| 长按 UP | 每 50 ms 控制拍推进一句；选项、结局与 OP 处停 | - |
| 长按 DOWN | 开关自动阅读 | - |

## 实现说明

- 30 个手动存档位保存节点、帧索引、文本页与当前周目变量，记录稀疏、CRC32 校验，
  并绑定资源包身份。
- 永久进度（`loop_my`、`loop1`、`true`、`clear_true`、`titleBg`）存放在独立 NVS
  记录；读档、回退或章节预览不会回滚已完成的结局。
- 320×180 舞台内部按 256×144 解码，再以 1.25× 显示，下方是 60 px 对话框。
- 鉴赏室从本地资源包播放 OP：源为 599 帧 / 6 fps，固件播放 300 帧 / 3 fps，并使用
  原始音轨。

## 发布

- 分支：`feature/nine-kokoiro-20261125`
  （<https://github.com/Shinku-Chen/ai-passport/tree/feature/nine-kokoiro-20261125>）
- Release：`v1.0.0-nine-kokoiro`
  （<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-nine-kokoiro>）
- 社区条目：<https://ai-passport.folotoy.cn/plays/9-nine>
- 固件：`FoloToy-AI-Passport-full.bin`，**5,405,040 B**，分区占用 **64.94%**，
  剩余 **2,918,032 B**

## 上游

- 来源：[`xuanyide01/9-nine-rw5`](https://github.com/xuanyide01/9-nine-rw5)，
  固定提交 `6a8907a5ae576f4377abe4b74f15bd9af13efe07`。
- 框架：本 fork 的 `feature/galgame-framework`。
