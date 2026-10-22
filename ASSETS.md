# AI PASSPORT × WorkBuddy — 资产梳理

> 一次会话内从零做成的东西清单：做了什么、在哪、怎么验、坑在哪。
> 最后更新：2026-10-21 21:25

---

## 1. 一句话定位

把 FoloToy AI PASSPORT（ESP32-C3 卡片机）改造成 **WorkBuddy 的物理遥控器**：

| 能力 | 说明 |
|---|---|
| **说话** | 按住 OK 说话 → PC 侧离线识别 → 文字自动进 WorkBuddy 输入框并发送 |
| **看见** | WorkBuddy 每发生一件事（会话开始/提交/工具动作/结束），屏幕自动更新 |
| **拍板** | 危险工具调用先弹到设备，按 OK 才放行（PreToolUse hook） |
| **听答** | 结论用设备扬声器念出来（8k IMA-ADPCM 推流） |

不是"AI 音箱"，是**输入输出双向的物理终端**。

---

## 2. 三层架构

```
        ┌──────────────────────────────────────────────┐
        │  WorkBuddy（Electron）                        │
        │   hooks ──┐                    MCP ──┐       │
        └───────────┼──────────────────────────┼───────┘
                    │ 127.0.0.1:37821          │
        ┌───────────▼──────────────────────────▼───────┐
        │  wb 服务（唯一持有串口，Python）               │
        │  devlink/terminal.py  语义层                  │
        │  devlink/inject.py    UIA 注入                │
        │  devlink/speech.py    识别（常驻子进程）       │
        └───────────────────┬──────────────────────────┘
                            │ USB-CDC 115200，单行 JSON
        ┌───────────────────▼──────────────────────────┐
        │  ESP32-C3 固件（LVGL + ES8311）               │
        │  屏 / 麦克风 / 扬声器 / 三键                  │
        └──────────────────────────────────────────────┘
```

**为什么必须单一服务占串口**：串口是独占资源；hooks 每个事件起一个进程、MCP 每会话一个进程，谁都想 `open("COM4")` 必然互相踢掉。所以其它一切只走 TCP 说一句话。

**为什么 hook 用 Node 不用 Python**：hook 是每事件起进程，本机 Python 冷启动实测 1.0–1.5s、Node 0.28s。这些延迟直接叠在 WorkBuddy 响应上。

---

## 3. 代码资产

### 3.1 固件（约 3800 行 C）

| 路径 | 行数 | 作用 |
|---|---:|---|
| `main/workbuddy_companion.c` | 1344 | **核心**：UI 布局、按键状态机、采集/播报任务、通知层、审批层 |
| `components/wb_protocol/src/wb_protocol.c` | 794 | 单行 JSON 协议编解码（C 侧零依赖手写解析） |
| `components/bsp/src/bsp_audio.c` | — | ES8311 采集/播放、**运行时增益 `bsp_audio_set_in_gain()`** |
| `components/bsp/src/bsp_button.c` | — | ADC 分压三键，产出 PRESS/CLICK/DOUBLE/LONG/RELEASE 五类事件 |
| `main/fonts/lv_font_wb_zh_16.c` | — | **自动生成** 中文字库，8058 码点，5.9MB 源码（编译后约 640KB） |

关键设计：
- **采集边录边发**：原来攒 12s 需要 80KB 缓冲，C3 无 PSRAM 撑不住
- **播报环形缓冲**：32KB = 2s @8k/16bit，发送领先 1.2s 防溢出
- **ADPCM 编解码重写**：状态跨帧连续 + 每块写起始预测器头（原实现每帧重置，PC 端解出来是乱的）
- **通知层不再拦截按键**：以前 `if (dialog || s_notif_overlay) return;` 导致每轮结束后的 5 秒内按 OK 只关通知、不录音

烧录产物：`build/FoloToy-AI-Passport.bin`，**1.56 MB**（分区 0x7F0000 = 8MB，余量充足）

### 3.2 主机侧 `host/devlink/`（约 3000 行 Python）

| 模块 | 行数 | 职责 | 关键点 |
|---|---:|---|---|
| `terminal.py` | ~700 | 语义层：按键/音频 → 该做什么 | 电平自检、增益自适应、超时中和残留 |
| `digest.py` | ~180 | transcript → 可朗读短句 | 取最后一条回复、清 markdown、句末截断 |
| `inject.py` | ~600 | 把文字送进 WorkBuddy 输入框 | `WM_CHAR` → 渲染子窗口，见 §6 |
| `server.py` | ~300 | 单实例服务 + TCP 控制口 | 端口占用即单实例锁 |
| `codec.py` | ~230 | ADPCM / 重采样 / 语音增强 | **开头瞬态裁剪**、300–3400Hz 带通 |
| `speech.py` | ~200 | 识别门面（常驻子进程） | 4 字节长度 + PCM 管道协议 |
| `asr_worker.py` | ~420 | 识别工作进程 | SenseVoice / Paraformer / SAPI 三后端 |
| `link.py` / `proto.py` | ~220 | 串口收发 / 消息常量 | 断线自愈 |
| `cli.py` | ~380 | 命令行（体检、录音、诊断） | 20+ 子命令 |
| `mcp_server.py` | ~120 | MCP 工具集 | 10 个工具 |

入口脚本：`host/server.py`（服务）、`host/hook.js`（hook）、`host/mcp_server.py`（MCP）、`host/start.cmd`（双击启动）

### 3.3 工具链 `tools/`（约 2000 行）

**能重建资产的关键工具**（不是一次性脚本）：

| 工具 | 作用 |
|---|---|
| `gen_font.py` | 用系统黑体生成中文字库 —— **换字/加字必须重跑它** |
| `check_glyphs.py` | 校验字库覆盖（当初靠它发现缺 45 个字） |
| `dl_sensevoice.py` / `dl_paraformer.py` / `dl_vad.py` | 下模型（分块校验 + 重试） |
| `verify_firmware.py` / `archive_firmware.py` | 固件校验与归档 |
| `check_repo.py` | 仓库结构检查 |
| `render_pdf.py` | Markdown → PDF（Chrome 无头打印，A4 中文排版） |
| `check_vault_links.py` | Obsidian 库断链自检（扫 `[[wikilink]]` 与全库文件名比对） |
| `run.py` | 脚本运行包装（本会话 Git Bash 损坏时的唯一可靠执行方式） |

**诊断工具**（都不碰输入框）：

| 工具 | 用途 |
|---|---|
| `cli log N` | 最近 N 条设备事件（**第一入口**） |
| `cli rec N` / `cli play raw\|clean\|voice` | 录音+识别+存盘 / 回放三次录音 |
| `cli purge` | 清 WorkBuddy 输入框（走 WM_CHAR，React 同步） |
| `tools/analyze_clip.py` | 逐 100ms 看 rms/削顶分布 |
| `tools/loopback.py` | 受控回环：PC 朗读固定句 → 设备录 → 识别比对 |
| `tools/calib_gain.py` | 自动校准增益 |
| `tools/diag_level.py` | 底噪 vs 信号，算信噪比 |
| `tools/measure_uplink.py` | 测上行吞吐 |
| `tools/inspect_queue.py` / `purge_queue.py` | 只读检查 / 清 WorkBuddy 待发队列 |
| `tools/cmp_engines.py` | 同音频对比两个识别引擎 |
| `tools/purge_queue.py` | 清幽灵消息队列 |

### 3.4 配置与集成点

| 位置 | 内容 |
|---|---|
| `~/.workbuddy/settings.json` | 5 类 hooks：`SessionStart` `UserPromptSubmit` `Notification` `Stop` `PreToolUse` |
| `~/.workbuddy/mcp.json` | `ai-passport-companion` → `host/mcp_server.py`，`WB_ASR_BACKEND=sense` |
| `host/.env` | 串口、增益 30dB、识别后端、审批超时 |
| 开机自启 | `Startup\wb-daemon.cmd` → `pythonw.exe server.py`（无黑窗） |

MCP 工具 10 个：`wb_ping` `wb_status` `wb_reply` `wb_speak` `wb_notify` `wb_ask` `wb_listen` `wb_type` `wb_last_heard` `wb_clear`

### 3.5 模型资产（`host/models/`，共 307MB）

| 模型 | 大小 | 用途 |
|---|---|---|
| `sense-voice/` | 228MB (int8) | **默认识别引擎**，带标点+数字归一化，每句 0.4s |
| `sherpa-onnx-paraformer-zh-small-2024-03-09/` | 78MB | 兜底引擎，清晰音频逐字准确 |
| `silero_vad.onnx` | 0.6MB | 语音活动检测（`WB_VAD=silero` 可选） |

---

## 4. 协议

USB-CDC，115200，**单行 JSON**（`{"type":"...","payload":{...}}\n`）。

### PC → 设备
`wb_status` 状态+文字 · `wb_reply` 对话行 · `wb_notification` 通知卡 · `wb_ask` 审批框
`wb_clear` 清屏 · `wb_speak` 播报（8k ADPCM 分片） · `wb_debug_talk` 远程开关麦 · `wb_mic_gain` 调增益

### 设备 → PC
`wb_key_event` 按键（`ok/up/down` × `press/click/release`） · `wb_audio` 音频分片 · `wb_audio_end`
`wb_answer` 审批结果 · `wb_pong`（带 `fw=wb-2.0` 版本标识）

### 关键实现细节
- **`usb_serial_jtag_write_bytes()` 会部分写入** —— 必须循环补齐，否则每条消息尾部被吃掉
- 上行音频帧：`[seq(1)][len(2 LE)][adpcm block]`
- 上行实测 **11.4 KB/s ≈ 1.0× 实时**（50ms/帧，19.8 帧/秒）

---

## 5. 已验证能力（都有实测证据）

| 项 | 状态 | 证据 |
|---|---|---|
| 编译 / 烧录 | ✅ | app 1.56MB，flash RC=0 |
| 中文屏显 | ✅ | 8058 码点，界面词零缺字，肉眼确认无方框 |
| 链路 | ✅ | `ping` → `fw=wb-2.0` |
| **语音全链路** | ✅ | 21:02 首次完整走通：press → 44 000 采样 → 识别 → 注入 → 发送 |
| **语音识别准确** | ✅ | 22:14「接下来我们可以做什么？」11 字一字不差；同一段音频 3 次结果一致 |
| 上行带宽 | ✅ | 11.4 KB/s ≈ 1.0× 实时 |
| 识别质量（清晰音频） | ✅ | 合成语音逐字全对 |
| 注入 | ✅ | `WM_CHAR` 走渲染子窗口，React 同步，能发出 |
| 自动发送 | ✅ | 日志「注入结果：ok（N 字）」 |
| 审批三分支 | ✅ | 超时→`ask`、只读→`allow`、拒绝→`deny` |
| hook 事件 | ✅ | 5 类全部到位，`← reply 你：…` 在屏上出现 |
| **每轮播报结论** | ✅ | Stop hook → transcript → 播报 56872 采样 / 7.1s；hook 侧 234ms |
| 单测 | ✅ | `python -m tests.test_codec` 5/5 |

---

## 6. 踩过的 10 个根因（这才是真正的资产）

按"最隐蔽 → 最明显"排，每条都是**看着通过、实际不工作**的类型。

### ① LVGL 内置中文字库只覆盖 1456 码点
`source_han_sans_sc_16_cjk` 名字像"常用汉字"，实际只有 1456 个码点，"确认/拒绝/说话/就绪/提醒/关闭"**全缺**。
缺字 LVGL 画占位方块 → 能编译能烧能通串口，屏幕一片方框。
→ `tools/gen_font.py` 生成 GB2312 全集（8058 码点），`check_glyphs.py` 做覆盖校验。

### ② `SendInput` 的 `INPUT` 结构尺寸错了
union 只放了 `KEYBDINPUT`（24B）→ `sizeof(INPUT)=32`，而 Windows 要求按最大成员 `MOUSEINPUT` 算成 **40**。
尺寸不对 `SendInput` 返回 0 **静默失败**。

### ③ `usb_serial_jtag_write_bytes()` 会部分写入
原代码只调一次，每条音频 JSON 尾部被截掉 → 整包解析失败。
这就是"CDC 只有 2KB/s"误判的真因。

### ④ hook 写出去的数据全被吞
`hook.js` 在 socket `connect` 回调里就 resolve，`main()` 随即 `process.exit(0)`。
此时 `write()` 只是进了内核缓冲，进程被强杀 → **一个字节都没发出去**。
表现：hooks 一直被调用，服务端从未收到；屏幕只在我手动 `cli` 推时才更新。
→ 必须等 socket `'close'`（FIN 发出、对端读完）。

### ⑤ `ValuePattern.SetValue` 改 DOM 但不触发 `input` 事件
React 受控状态不更新 → **文字看得见，但回车不发送、退格删不掉，刷新才恢复**。
→ 改用 `WM_CHAR` 发到渲染子窗口（见 ⑥⑦⑧）。

### ⑥ `WM_CHAR` 必须发给渲染子窗口
`Chrome_RenderWidgetHostHWND`，不是主窗口。发给主窗口完全无效（编辑逻辑在 renderer 里）。
我前面一直发错对象，才误判"键盘注入完全不可行"。

### ⑦ 没焦点时 `PostMessage` 返回 1 但字一个不进
必须先用 UIA `SetFocus()`。文档说它只设无障碍焦点，但 Chromium 会响应。
**这是最坑的一条：假成功。**

### ⑧ 空输入框的 `ValuePattern.Value` 返回 placeholder 文本
不是空串，而是 `今天帮你做些什么？ @ 引用对话文件…`。
判空必须排除它，否则"发出去了"会被判成"文字还在"。

### ⑨ 窗口被遮挡 → Chromium 销毁整棵无障碍树
`render_hwnd=0`，UIA 读不到任何 `EditControl`。切前台 0.5s 后恢复。
→ 注入前必须 `ensure_ready()`：切前台 → 轮询等树重建 → **清缓存重扫**。

### ⑩ 两个"清"完全不同
`cli clear` 清的是**设备屏幕**；`cli purge` 清的是 **WorkBuddy 输入框**。
我一直以为在清输入框，其实只清了屏幕 —— 那句旧文本（DOM 空但 React state 还在）一直躺着，用户一按回车就发出去，同一条消息反复出现好几次。

> 外加一条操作事故教训：**绝不在用户真实输入框里做注入测试**。
> 即使事后清了输入框，进了 WorkBuddy 待发队列的消息还会一个个自己发出去，表现就像"幽灵消息"。

---

## 7. 语音这条链的实测参数

| 参数 | 值 | 依据 |
|---|---|---|
| 麦克风增益 | **30 dB** | 15dB 时 rms 只有 25（等于没录到），30dB 时 rms 278、peak 6161 |
| 静音阈值 | `rms < 400` 判静音 | 底噪 180–230，真实近距离说话 2 500+ |
| 削顶分级 | ≥3‰ 仍识别，≥25‰ 拒绝 | 实测 2‰ 时 10 个字全对 |
| 开头瞬态 | 切掉（最后满幅样本 +20ms） | 每次开录头 100ms 有 464 个满幅样本，rms 23077 |
| 带通 | 300–3400 Hz（FFT 软边） | 弱信号下识别明显更连贯 |
| 识别引擎 | SenseVoice（int8，228MB） | 自带标点+数字归一化，每句 0.4s |

**关键事实**：识别引擎本身是准的。失败的那些 rms 只有 229（底噪水平），设备根本没听到声音。
同一套参数在安静+近距离下 100% 正确。

---

## 8. 排障速查

### 按 OK 没反应
```powershell
& $V -m devlink.cli log 20
```
| 日志现象 | 结论 |
|---|---|
| 无 `按键 ok/press` | 按键没触发：通知层/审批层挡着，或按得太短（<450ms） |
| 有 press，无 `音频开始上行` | 麦克风没采到：codec 或增益问题 |
| 有音频但无 `收到语音` | 上行被截断：检查 `serial_send` 循环补齐 |
| 有 `收到语音` 但识别乱 | 信号弱或环境吵：看 `电平自检` 的 rms |

### 识别不准
```powershell
& $V -m devlink.cli rec 5          # 看电平
& $V -m devlink.cli play voice     # 回放上次真实语音 —— 判断"没听清"还是"说错了"
& $V $T\analyze_clip.py            # 逐 100ms 看削顶
```

### 屏幕不跟着动
hooks 没加载 → 重启 WorkBuddy。确认：`settings.json` 的 `hooks` 有 5 个事件。

### 文字进不去输入框
```powershell
& $V -m devlink.cli inject "测试这句话"
```
它会打印窗口句柄、渲染子窗口、输入框位置、底部按钮、是否在回话、最终结果。

---

## 9. 已知限制 / 下一步

| 限制 | 说明 | 建议方向 |
|---|---|---|
| 注入会抢前台 | 被遮挡时无障碍树被销毁，必须先切前台 | 可接受：说话本来就是发给 WorkBuddy |
| 退格无效 | Chromium 删字符要真实键盘事件，`WM_CHAR 0x08` 无效 | 清空只能"选中全部 + 覆盖"，已实现 |
| 待发队列会自己发 | WorkBuddy 空闲时自动排队消息排一条 | 测试绝不留残留；`inspect_queue.py` 检查 |
| PreToolUse 默认关 | 每个写操作都阻塞等人按键，日常太烦 | 建 `host\.approval_on` 开启 |
| 弱信号识别差 | rms < 500 时不可靠 | 靠近说（20cm）；或加降噪模型（gtcrn，网络没下成） |
| 本地 Whisper 不可用 | 本机 `ctranslate2`/`torch` 载入即崩（两个 Python 环境都测过） | 已放弃，走 sherpa-onnx |

---

## 10. 目录速查

```
ai-passport/
├─ README_WORKBUDDY.md     ← 完整技术说明（先看这个）
├─ ASSETS.md               ← 本文件：资产清单
├─ build_stream.py         ← 编译入口（python build_stream.py）
├─ build_wrapper.py        ← 编译包装（idf.py 参数）
├─ partitions.csv  sdkconfig.defaults
├─ main/                   ← 固件业务代码 + 自动生成字库
├─ components/
│  ├─ bsp/                 ← 板级驱动（音频/显示/按键/电池/I2C）
│  └─ wb_protocol/         ← 单行 JSON 协议
├─ host/
│  ├─ devlink/             ← 主机六层包（11 模块）
│  ├─ hook.js              ← WorkBuddy hook 入口（Node）
│  ├─ server.py            ← 服务入口
│  ├─ mcp_server.py        ← MCP 入口
│  ├─ .env                 ← 配置
│  ├─ models/              ← 307MB 模型
│  ├─ recordings/          ← last_raw / last_clean / last_voice
│  ├─ events.log           ← 持久事件日志（cli log 读它）
│  └─ start.cmd            ← 双击启动
├─ tools/                  ← 生成/下载/诊断工具
├─ tests/                  ← 单测（C 桩 + Python）
├─ skills/                 ← 项目自带 skill
├─ docs/                   ← 厂商文档（硬件设计指南等）
├─ _attic/                 ← 已归档的一次性脚本（可还原）
└─ build/                  ← 编译产物（FoloToy-AI-Passport.bin）
```

**归档说明**：`_attic/2026-10-21-idf-bringup/` 里是 72 个一次性文件（ESP-IDF 初次安装、
反复 clone/checkout、构建尝试的脚本与日志）。都是搭建环境的中间产物，运行期不需要。
需要回溯时直接移回根目录即可：`move _attic\2026-10-21-idf-bringup\* .`

### Obsidian 里的知识版

项目知识已归档到 Obsidian 库 `G:\AI WORKSPACE\claudecode应用`，按该库的 LLM Wiki 规范放置：

| 位置 | 内容 |
|---|---|
| `40-工作流案例/AI-PASSPORT硬件伴侣/` | 六册：项目总览 / 架构与协议 / **十个根因** / 语音链路调优 / 运维手册 / 资产清单 + 打印版 PDF |
| `wiki/entities/AI-PASSPORT硬件伴侣.md` | 实体页（项目在知识网络里的锚点） |
| `wiki/concepts/硬件终端驱动AI-Agent.md` | 概念页 —— **可迁移的方法论**（三类假成功模式） |
| `wiki/index.md` | 已加入「工作流案例」区块与入口 |
| `wiki/log.md` | 已追加本次 ingest 记录与遗留缺口 |

区别：**仓库里的是"怎么修"，Obsidian 里的是"怎么判断"。**
前者给动手的人，后者给下次遇到同类问题的人。

---

## 11. 从零重建的最短路径

```powershell
# 1. 固件
python build_stream.py                     # 约 2 分钟
python "C:\Users\46755\.workbuddy\skills\folotoy-flash\scripts\flash.py" COM4

# 2. 主机环境
cd host
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt
python ..\tools\dl_sensevoice.py           # 228MB，约 3 分钟

# 3. 配置
#   ~/.workbuddy/mcp.json     加 ai-passport-companion → host/mcp_server.py
#   ~/.workbuddy/settings.json 加 5 类 hooks → host/hook.js
#   ★ 重启 WorkBuddy 让 hooks 生效

# 4. 起服务
host\start.cmd                             # 或装开机自启
& $V -m devlink.cli doctor                 # 体检
```
