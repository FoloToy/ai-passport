<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 死馆 阅读器

AI Passport 上的 240×320 竖屏视觉小说阅读器。固件内置剧情、立绘、CG 差分与选项，
全程离线；源数据没有语音、BGM 与 OP 视频，因此是无声阅读器。

## 规模

- 198 个运行页、17,409 条文本记录。
- 11 组选项、56 组 / 311 个 CG 差分。
- 3 个手动存档位 + 自动进度；80 px 对话框在两种字号下都显示三行。

## 操作

| 按键 | 阅读中 | 列表 / 选项 |
| --- | --- | --- |
| 短按 OK / UP / DOWN | 补全打字动画，再推进一页 | 移动光标 / 确认 |
| 长按 OK | 打开菜单 | 返回上一层 |
| 长按 UP | 跨章节立即快进；松手、选项与结局处停止 | - |
| 长按 DOWN | 开关自动阅读；选项处也保持屏幕常亮 | - |

菜单提供 3 个手动存档位、读取、历史、字号、文字速度与对话框透明度。标题页的
「继续」读取自动进度。鉴赏室包含已解锁 CG、回想与章节入口。

## 实现说明

- 源数据没有语音、BGM 或 OP 视频，所以本作是无声阅读器；源中未解析或不可达的节点
  被保留，而不是用虚构内容替代。不独立保证商业原作的完整度。
- 说话人徽章按文本长度自适应，对话框在两种字号下都保持三行。
- 竖屏构建、横屏实验与缓存修复记录在
  [`docs/assets/shinuyakata.md`](https://github.com/Shinku-Chen/ai-passport/blob/feature/port-shinuyakata/docs/assets/shinuyakata.md)。

## 发布

- 分支：`feature/port-shinuyakata`
  （<https://github.com/Shinku-Chen/ai-passport/tree/feature/port-shinuyakata>）
- Release：`v0.1.3-shinuyakata`
  （<https://github.com/Shinku-Chen/ai-passport/releases/tag/v0.1.3-shinuyakata>）
- 社区条目：<https://ai-passport.folotoy.cn/plays/galgame-4>
- 固件：`FoloToy-AI-Passport-full.bin`，**8,180,560 B**，分区占用 **98.29%**，
  剩余 **142,512 B**

## 上游

- 来源：[`xuanyide01/shinuyakata-rw5`](https://github.com/xuanyide01/shinuyakata-rw5)
- 框架：本 fork 的 `feature/galgame-framework`。
