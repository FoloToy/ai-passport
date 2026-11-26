<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 星光咖啡馆与死神之蝶 阅读器

AI Passport 上的 320×240 横屏视觉小说阅读器。固件内置剧情、美术、音频、OP 视频、
七个主线结局与五个后日谈，全程离线。

## 规模

- 923 个剧情节点、53,727 帧对白。
- 69 组选项 / 141 个选项；7 个主线结局（bad / normal / Kanna / Natsume / Nozomi /
  Mei / Suzune），5 个后日谈，189 个章节入口。
- 121 组 / 1,631 个 CG 差分；1,850 张源图（1,729 张背景 + 121 张立绘底图，另有
  140 张表情图集）。

## 操作

| 按键 | 阅读中 | 列表 / 选项 |
| --- | --- | --- |
| 短按 OK / UP / DOWN | 立即推进；跳过打字与自动等待 | 移动光标 / 确认 |
| 长按 OK | 打开菜单 | 返回上一层 |
| 长按 UP | 快进；松手立即停，选项与结局处停住且不替玩家选择 | - |
| 长按 DOWN | 开关自动播放 | - |

## 实现说明

- 源 432×243 舞台按 224×126（精确 16:9）打包，运行期用三条 224×42 RGB565 条带
  由 LVGL 缩放到 320×180；不分配整屏 RGB565 画布。
- 背景使用 JPEG q10；立绘与表情图集使用 q18 + 2 bpp alpha 遮罩（4 级），透明 RGB
  用最近的不透明边缘填充。
- 中文字体为 16 px / 20 px 4 bpp 子集，覆盖资源包与 UI 中实际出现的 3,401 个码位，
  不使用系统回退字体。
- 完整保留音频与视频：标题曲 256.8 秒，OP 为 92.995 秒音频 + 95.46 秒视频。OP 复用
  三条 224×42 条带，不增加整帧分配。

## 发布

- 分支：`feature/cafestella-20261125`
  （<https://github.com/Shinku-Chen/ai-passport/tree/feature/cafestella-20261125>）
- Release：`v1.0.0-cafestella`
  （<https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.0.0-cafestella>）
- 社区条目：<https://ai-passport.folotoy.cn/plays/community-0c95d859>
- 固件：`FoloToy-AI-Passport-full.bin`，**8,312,208 B**，分区占用 **99.87%**，
  剩余 **10,864 B**

## 上游

- 来源：[`xuanyide01/cafestella-rw5`](https://github.com/xuanyide01/cafestella-rw5)，
  固定提交 `90ddb9e020970907b61ad4181f25c65267e6e98b`。
- 框架：本 fork 的 `feature/galgame-framework`。
