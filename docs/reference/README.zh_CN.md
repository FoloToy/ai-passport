<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 参考（Reference）

本目录存放 AI Passport 开发中**不构成硬性要求**的参考资料：可复用的开发经验与已归档的应用档案。开发新东西时参考它们，而不是把它们当作强制规则。参考资料按贡献者的 GitHub 用户名组织：每个相对仓库根目录的 `docs/reference/<username>/` 目录下，经验条目以平铺文件存放，应用档案以子目录存放。

工程规则本身位于 [`../development/`](../development/README.zh_CN.md)；协作规范位于 [`../contribution/`](../contribution/README.zh_CN.md)。

## 贡献者

### Shinku-Chen

**经验条目：**

- [ESP32-C3 上音频压缩方式的权衡](shinku-chen/audio-compression-trade-offs.zh_CN.md) — 在有限 Flash 上如何为语音播放应用选编解码（IMA-ADPCM vs Opus vs MP3），含实测容量与解码器成本。
- [发布后收尾：AI Passport 发布流程的衔接](shinku-chen/post-release-follow-up.zh_CN.md) — 确认发布目的地、发布时包含数据分区、以及发布后收尾各轨道的同意门槛。
- [ESP32-C3（无 PSRAM）上的显示刷新与深睡](shinku-chen/display-refresh-and-deep-sleep.zh_CN.md) — 直接刷新单个图片矩形、RTC GPIO 深睡唤醒，以及 LVGL 对象类型误用的崩溃特征。
- [深睡前关闭板载外设](shinku-chen/deep-sleep-peripheral-power-off.zh_CN.md) — 寄存器校验关闭、共享总线顺序、终端 GPIO 状态、LCD deep-sleep hold、`esp_codec_dev_close()` 打开状态陷阱及剩余硬件负载。
- [横屏旋转与深睡按键唤醒](shinku-chen/landscape-rotation-and-deep-sleep-key-wake.zh_CN.md) — 通过 LVGL 把竖屏面板转成 320 × 240 横屏、圆角遮罩为何必须跟随逻辑分辨率，以及被 ADC 占用的引脚如何让低电平深睡唤醒在入睡瞬间就成立。
- [设备端对弈 AI 的墙钟预算](shinku-chen/on-device-game-ai-wall-clock-budget.zh_CN.md) — 为什么节点数上限在这块板上会跑偏（约每秒 1.5 万节点）、用时间预算迭代加深、让出 CPU 避免饿死空闲任务，以及用失误率表达难度。
- [静态缓冲按面板算，并验证已发布的产物](shinku-chen/release-artifact-verification.zh_CN.md) — 一处 51KB 缓冲错误导致空闲堆只剩 8KB、读已发布合并镜像的启动日志，以及替换刚发布的版本而不是另发后续版本。
- [无 PSRAM 的 AI Passport 双机 BLE 联机](shinku-chen/two-device-ble-link.zh_CN.md) — 用地址大小做对等发现、把角色选择从界面里去掉；无 PSRAM 上联机的实测堆开销及其与静态截图缓冲的冲突；两个只在真机暴露的 NimBLE GATT 陷阱（缺 `access_cb`、订阅成功后的 `EDONE`）；NVS 与射频校准；回合制对战的停等可靠层。

**应用档案：**

- [音效钥匙扣](shinku-chen/voice-keychain/README.zh_CN.md) — 把 AI Passport 变成口袋音频播放器的音效钥匙扣。
- [今天吃啥](shinku-chen/eat-what/README.zh_CN.md) — 按键驱动的食物轮盘，把 AI Passport 变成「今天吃什么」小转盘。
- [四子棋](shinku-chen/connect-four/README.zh_CN.md) — 横屏 10 列 × 7 行的四子连珠游戏，带三档电脑难度、双人模式、合成音效与空闲自动深睡。

### PhoenixZHC

**经验条目：**

- [AI Passport 网络音频流与内存预算经验](phoenixzhc/network-audio-streaming-and-memory.zh_CN.md) — 有边界的 HTTP 音频流、ES8311/I2S 资源归属，以及解码、JSON、DMA 与 LVGL 的统一内存预算。
- [AI Passport SoftAP 配网与资源预算经验](phoenixzhc/softap-provisioning-and-resource-budget.zh_CN.md) — DHCP 状态、弹窗认证兼容、表单与上传边界，以及无 PSRAM 条件下的资源规划。

### Y2Lin

**经验条目：**

- [实现 FAP_SCREENSHOT_V1 串口截屏协议](y2lin/serial-screenshot-protocol.zh_CN.md) — 先装 USB-Serial-JTAG 驱动、按子串匹配命令、快照渲染进静态整屏缓冲、按发送环形缓冲分块流载荷、二进制窗口内静默日志。
- [音量计 UI：读数平滑、动画锚定与杂色块](y2lin/meter-ui-smoothing-and-layout.zh_CN.md) — 非对称 EMA 平滑实时读数、吉祥物动画锚定到创建位置、屏上杂色块的常见根因，以及 LVGL 池耗尽导致开机白屏。

### sunny0826

**应用档案：**

- [离线宝可梦图鉴](sunny0826/offline-pokedex/README.zh_CN.md) — 把全部 1025 只宝可梦与精灵、叫声内嵌固件的全离线图鉴。

## 新增经验条目

一次发布可沉淀**一条或多条**可复用经验，每条作为独立条目新增，以发布版本（tag 或 commit）作为上下文。遵守仓库语言规则：默认 `.md` 路径用英文、配套 `.zh_CN.md` 用简体中文，并在同一次变更中对齐。

条目是放在 `docs/reference/<username>/` 下的单个 `.md`（及其 `.zh_CN.md`），按条目内容概要命名（小写连字符，例如 `audio-compression-trade-offs.md`），描述主题而非时间戳。每条经验在提交前分流：通用、上游也受益的经验作为 PR 提交到上游 `FoloToy/ai-passport`；纯 fork 定制按 [`docs/fork-guide.md`](../fork-guide.zh_CN.md) 留在 fork。

## 归档应用

应用发布后，在相对仓库根目录的 `docs/reference/<username>/<app-name>/` 下归档，附一份 AI 生成的双语功能说明（`README.md` / `.zh_CN.md`），可选配一份使用指南。档案为**纯文本**——封面图仅记录文件名与格式，不存放固件 `.bin`。`plays-archive` skill 驱动归档及其约定。同一次变更中，在本索引及其英文版本中登记该应用。

## 相关

- 仓库总览与 demo 分支：[`../README.md`](../README.zh_CN.md)
