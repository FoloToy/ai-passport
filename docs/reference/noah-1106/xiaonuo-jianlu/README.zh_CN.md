<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 小诺简录（语音 AI 清单）

一套 AI 简录系统，把 AI Passport 变成"随时记录、随机整理"的语音清单卡片。
按住 OK 说话——中枢把语音转成文字并用 AI 归类整理成简录卡片；清单一叠卡片的
形式呈现，在卡片、Web 界面和 AI 助手之间实时同步。

## 发布信息

- **标题**：小诺简录 · 随说随记的 AI 清单 / Xiaonuo Jianlu · Talk-to-Note AI Lists
- **简介**：按住卡片说一句话，它就能听懂、归类、生成简录卡片；清单像一叠真实的
  卡片一样堆在屏幕上。断网也能记（语音存本地可回放），配合开源中枢多端同步。
- **封面**：`hero-cover.jpg`（JPEG，1152×1536）；配图 `card-recording-cover.jpg`、
  `card-confirm-cover.jpg`（JPEG，1152×1536）。

## 功能

- **按住说话记录**：长按 OK 说话松开，音频存入 SPIFFS 分区并流式上传到自托管
  中枢；中枢经 ASR 转写、LLM function calling 建/整理简录（类型、标题、标签、
  摘要），确认页显示新卡片。
- **堆叠卡片清单**：清单一叠卡片呈现——顶卡大卡片（类型徽标、标题、摘要、标签、
  日期），后卡内收渐暗；UP/DOWN 翻卡带飞出过渡，OK 完成顶卡。
- **离线模式**：中枢/网络不可达时加载本地快照照常浏览，勾选记"待同步"排队，
  离线录音以占位卡形式入列（"语音 · 未识别"，可本地回放）；联网后全部自动同步。
- **配网**：BLUFI（EspBlufi App）下发 Wi-Fi 凭据（存 NVS）；中枢经 mDNS
  （`_xiaonuo._tcp`）在局域网自动发现，零手动配置。UP 长按重配。
- **省电**：60 秒空闲熄屏、10 分钟深睡，任意键唤醒且唤醒手势不触发业务。
- **中枢**：开源 Express + SQLite 服务
  （<https://github.com/noah-1106/xiaonuo-jianlu>），OpenAI 兼容 LLM/ASR 适配层、
  Web 界面、MCP 端点、Agent Skill——一份数据四个入口。

## 交互

三键驱动。顶栏显示标题与电量。

- **UP / DOWN**：翻卡片堆（长文翻页）。
- **OK**：完成顶卡 / 确认 / 回放待识别语音。
- **OK 长按 ≥0.5s**：按住说话，松开发送。
- **OK 双击**：手动刷新。
- **UP 长按**：重新配网（确认后清除已存 Wi-Fi 与中枢信息）。

## 工程要点（参考价值）

- **配网与联网分离**：本板内存无法让 Wi-Fi 与 NimBLE 共存，配网态纯 BLE 收凭据
  存 NVS 后重启，走纯 Wi-Fi 路径。
- **严苛路由器下的 mDNS**：部分路由器不向设备转发组播应答，发现流程用手写
  mDNS 查询强制单播直答（RFC 6762 §8.1）；中枢用系统级 responder 广播保证兼容。
- **工作域看门狗**：任务仅在工作时注册 TWDT、空闲时注销——空闲永不误触发，
  进出 light sleep 时同步摘挂防竞态。
- **应用中文字体**：内置 CJK 子集仅 1187 字，远不够 ASR 任意文本；用
  `lv_font_conv` 从 Noto Sans SC（OFL）生成 GB2312 覆盖的 16px 字体（约 7 千字）
  并随仓库记录复现命令；emoji 在字形层静默丢弃。
- **无 PSRAM 内存纪律**：音频 DMA 缓冲在开机堆宽裕时预分配（延迟分配正是录音
  卡死的根因），HTTP 在独立任务用静态有界缓冲，稳态堆保持约 33KB。

## 源码

- **固件**：<https://github.com/noah-1106/ai-passport/tree/feature/xiaonuo-jianlu>，
  入口 `main/jianlu_app.c`（UI `main/jianlu_ui.c`、录音 `main/jianlu_capture.c`、
  配网 `main/jianlu_provision.c`）
- **中枢**：<https://github.com/noah-1106/xiaonuo-jianlu>（Express + SQLite，
  Web 界面、MCP、Agent Skill）
