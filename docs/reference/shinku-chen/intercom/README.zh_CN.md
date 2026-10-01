<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 随身 AI 对讲机（Pocket Intercom）

把 AI Passport 变成一台挂在手机上的 AI 对讲机：按住 **OK** 说话，配套的安卓应用把你的话交给局域网里的 AI 助理，再把回答送回设备小屏和手机。设备本身不联网、不解析 HTTP、也不保存任何网关密钥 —— 手机端是中介，音频以 Opus 走蓝牙，约 3 KB/s。

## 发布信息

- **标题**：Openclaw/OpenAI 随身对讲机（英文标题 Pocket Intercom）
- **简介**：提交内容为：

  > 把 AI Passport 变成一台随身对讲机：按住 OK 对着它说话，手机上的配套应用会把你的话交给你的 AI 助理，再把回答送回这块小屏幕和手机里。
  >
  > 手机端作为中介，连接你的对讲机和后端 AI：后端可以是 OpenClaw，也可以是兼容 OpenAI 的 Hermes 等接口，在应用里切换即可；只要手机能访问到，设备就能用。设备本身不联网、也不用配 Wi-Fi。
  >
  > 玩法很简单：
  > - 按住 OK 就开始说：屏幕先变红表示正在准备，变绿就代表"可以说了"，松手即发送，随后回复会直接出现在设备屏上。
  > - 想追问就再按一次，像对讲机一样一来一回；同一段对话也会显示在手机应用里，方便回看。
  > - 设备上只有三个键：按住 OK 说话，短按 OK 只点亮屏幕，长按 UP 进设置，UP / DOWN 翻看历史对话。
  > - 适合不方便掏手机的场景：做饭时问一句火候、出门前问一句今天要不要带伞、陪孩子随口问个为什么；家里老人小孩也上手就会。
  >
  > 关于声音：当前版本还不会"说话"——AI 的回答只以文字显示在设备屏和手机应用里，设备不会把回答念出来。语音朗读可能在以后的版本里加上。
  >
  > 上手三步：给设备刷入本项目的固件 → 手机装配套应用并与设备配对 → 在应用里填好你的 AI 助理。用 OpenClaw 时，第一次连接还需要在管理端批准一次这台设备，应用和设备屏会直接提示怎么做。
  >
  > 文字有点长也不怕：回复会自动滚到底，按 UP / DOWN 就能往回翻。

- **分类**：productivity
- **标签**：`family`
- **封面**：`cover-icon-3x4.png`（PNG，1152 × 1536，3:4）—— 即 App 图标；此处仅记录发布元数据，图片不随本仓库提交。
- **固件**：`FoloToy-AI-Passport-full.bin`，2,335,216 字节，
  sha256 `3e145858611cc30ee8c482e82301a91a4025a13a859bd795c0569d1c0079c402`
  （发布 [`v1.8.0-intercom`](https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.8.0-intercom)）
- **社区作品**：<https://ai-passport.folotoy.cn/plays/799/>
- **源码（固件）**：<https://github.com/Shinku-Chen/ai-passport/tree/feature/openclaw-intercom>
- **源码（手机端 App，独立仓库）**：<https://github.com/Shinku-Chen/ai-passport-openclaw-android>

## 它做了什么

- **按住 OK 说话**：按键一按下屏幕立刻变红，网关就绪后转绿，松开 OK 即发送这一轮。实测按下到变绿约 **280–290 ms**，因为识别通道一直保持预热。
- **回答上屏 + 手机同步**：识别出的文字与助理的回答都显示在设备屏上；同一段对话在手机应用里也能看，历史记录也存在手机端。
- **三个按键**：按住 OK 说话、短按 OK 只点亮屏幕、长按 UP 进设置页（亮度、设备信息）、UP / DOWN 翻看历史对话。
- **手机端的网关抽象**：OpenClaw 走 WebSocket RPC + ed25519 设备身份；OpenAI 兼容 HTTP（Hermes 与自定义接口，支持完整请求路径）；另有 Echo 假网关用于只测链路。每种类型各自记住一份配置。
- **保存前校验**：手机应用在保存前会真的连一次，失败会给出可读原因，因此错误的 token 或路径不会被当成可用配置存下来。
- **网关授权闭环**：OpenClaw 要求每台设备先被批准一次；应用会显示「等待网关授权……（deviceId …）」，批准后每 5 秒自动重试、最多 180 秒。
- **只有文字**：本版本不做语音合成。回答只以文字显示；应用不会朗读，设置页也没有相关开关。
- **空闲行为**：闲置一分钟后背光关闭，按任意键恢复；蓝牙链路保持连接，所以下一次按下立刻可用。

## 交互

- **屏幕顶部**：两行状态 —— `设备` 与 `网关` —— 外加电量百分比。链路与网关都可用时两行都显示就绪。
- **对话区**：一轮一个气泡，最新的在底部。长回复自动滚动、始终可读；UP / DOWN 上下翻页。
- **录音过程中**：整屏先变红（正在准备），随后变绿（可以说了）。这一轮发送后回到普通界面。
- **设置页**（长按 UP）：背光亮度与设备信息页；短按 OK 或超时返回对话。
- **手机应用**：四个页签 —— 对话、概览、设备、设置。设备页负责配对与显示已连接设备；设置页里是网关类型及其字段。

## 数据与工具链

- **线协议**：`[A5 5A][TYPE][FLAGS][LEN:2B 大端][payload]`，带 magic 重同步规则；类型为 PCM `0x01`、文本 `0x02`、控制 `0x03`、事件 `0x04`、Opus `0x05`。权威文档在本仓库 `docs/development/engineering/intercom-wire-protocol.md`（英文）与 `.zh_CN.md`。
- **音频上行**：Opus，16 kHz 单声道、60 ms 帧（960 采样）、complexity 0、开 DTX，约 3 KB/s —— 约为裸 PCM 的十分之一。编码跑在静态任务栈上，并保留 PCM 回退帧类型。
- **蓝牙**：Nordic UART Service 特征，LE Secure Connections + 设备屏显示 6 位配对码，链路上带帧限流，避免把手机灌爆。
- **手机端**：Kotlin 安卓应用；语音识别走设备厂商同款云端通道，网关适配层见上。任何密钥都不会下发到设备，设备也不解析 HTTP。
- **字体与素材**：16 px 中文字体子集由固件分支里的脚本生成（`tools/intercom_font.py`）；App 图标由 App 仓的 `tools/make_android_icons.py` 从入库的母版图生成。

## 验证了什么

- 真机实测：按下到变绿 280–290 ms；Opus 上行约 3 KB/s；从说话到回答上屏的完整一轮，分别走 OpenClaw 网关与 OpenAI 兼容端点；6 位配对码配对；历史消息翻页。
- 已发布 `v1.8.0-intercom`，并以项目 799 上架社区市场，附真机实拍与 App 截图。
- **未覆盖**：蓝牙距离、续航、超长时间会话、并发轮次上限，以及 Hermes 端到端（当时它的 API server 未暴露到局域网）。语音合成只存在于休眠代码路径中，界面未开放。

## 来源

- 固件：分支 [`feature/openclaw-intercom`](https://github.com/Shinku-Chen/ai-passport/tree/feature/openclaw-intercom)，
  发布 [`v1.8.0-intercom`](https://github.com/Shinku-Chen/ai-passport/releases/tag/v1.8.0-intercom)。
- 手机端 App：<https://github.com/Shinku-Chen/ai-passport-openclaw-android>（Android 8 及以上；签名包由 CI 构建）。
- 社区作品：<https://ai-passport.folotoy.cn/plays/799/>。
- 图标与封面上的设备外观取自 AI Passport 硬件；屏幕上出现的角色是 OpenClaw 的吉祥物，不在本项目授予的任何许可之内。
