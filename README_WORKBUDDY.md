# AI PASSPORT × WorkBuddy

把 FoloToy **AI PASSPORT**（ESP32-C3 / 240×320 屏 / 麦克风 / 扬声器 / 上·下·OK 三键）
做成 WorkBuddy 的物理终端：屏幕上看得见它在干什么，按一下就能拍板，说话就能下命令。

## 已经踩掉的十个根因

每一条都只会在"看起来能跑"之后才暴露——所以之前一直"跑很久没结果"。

**① 中文根本显示不出来。**
LVGL 内置的 `source_han_sans_sc_16_cjk` 只覆盖 **1456 个码点**，不含"确认/拒绝/说话/
就绪/提醒/关闭"。缺字 LVGL 画占位方块 —— 固件能编译、能烧录、串口全通，屏幕一片方框。
→ `tools/gen_font.py` 用系统黑体生成 GB2312 全集字库（8058 码点）。
`tools/check_glyphs.py` 做覆盖校验，加任何界面词之前先跑它。

**② 麦克风增益每次开录都被改回默认。**
`bsp_audio_set_format()` 里写死了 `esp_codec_dev_set_in_gain(s_dev, 15.0f)`，
而录音开始必然重开 codec —— 于是运行时设的增益**永远被覆盖**。
这就是为什么之前"扫 8 档增益"结果像随机跳：根本不是随机，是我设完就被复位了。
→ 固件记住 `s_in_gain`，open 之后重新施加。现在 `WB_MIC_GAIN` 设一次就一直有效。

**③ 识别引擎选错了。**
Windows SAPI 听写引擎按实时速度吃音频，长句在 ~3.5s 处截断，而且大量返回空串
（实测 10 次里 7 次空）。这就是"识别效果很差"。
→ 换 **sherpa-onnx + Paraformer**（中文专用，离线）：5.6s 音频 **0.11s** 出结果，
实测「帮我把第三组的实验记录整理成表格」**一字不差**。

**④ 什么方式都试过了，最后只能用 WM_CHAR（四代做法全错）。**
1. 剪贴板 + Ctrl+V：窗口在前台 ≠ 焦点在输入框，粘贴常落在页面空白处。
2. `SendInput` / `keybd_event`：本进程被 UIPI 隔离，函数返回 1 但事件没派发；
   而且 UIA 的 `SetFocus()` 只设无障碍焦点，**不移动系统键盘焦点**。这是"假成功"，最难查。
3. UIA `ValuePattern.SetValue`：能改 DOM，但**不触发 `input` 事件**，React 受控状态不更新。
   症状是：字看得见，但按回车不发送、按退格删不掉，刷新一下才恢复。
4. 最终方案：往**渲染子窗口** `Chrome_RenderWidgetHostHWND` 发 `WM_CHAR`。
   这是真实字符输入路径，Chromium 当人打字处理，`input` 会触发，React 同步。
   注意：必须发给渲染子窗口，发给主窗口**完全无效**（编辑逻辑在 renderer 里）。

→ 提交用 UIA `Invoke` 点"发送"键。注意 WorkBuddy **正在回话时发送键不存在**
（那个位置换成"停止"），此时只能等它空闲再点。
→ 清空只能用退格字符 `WM_CHAR 0x08`，不能用 `SetValue`，否则 DOM 与 React 状态又分叉。

**⑤ 开机自启指向了已删除的脚本。**
`Startup\wb-daemon.cmd` 里写的还是 `wb_daemon.py`，早就被 `devlink` 取代了。
→ 已改为 `pythonw.exe server.py`（无黑窗口）。

**⑥ hook 发出去的数据全被吞掉了。**
`hook.js` 在 socket 的 `connect` 回调里就 `resolve`，`main()` 随即 `process.exit(0)`。
此时 `write()` 只是把数据放进内核缓冲，进程被强杀 → **数据一个字节都没发出去**。
所以 hooks 一直在被调用、事件一直在产生，但服务端从来没收到过一条：
屏幕只会在我手动 `cli` 推的时候才更新，其余时间像死的一样。
→ 改成等 socket `'close'`（FIN 发出去、对端读完）才结束，绝不提前 exit。
修完后逐条验证：`← clear` `← status 会话已开始` `← reply 你：…` `← notify` 全部到位。
这也是为什么服务端要记 `← op` 日志——没有日志，"hook 到底有没有在工作"根本无从判断。

**⑦ 注入不能改 DOM，必须走真实输入路径。**
用 UIA `ValuePattern.SetValue` 写输入框，能改 DOM 但**不触发 `input` 事件**，
React 的受控状态不更新。症状就是你看到的：**文字看得见，但按回车不发送、
按退格删不掉，刷新一下才恢复**（刷新后 React 用旧状态重渲染，文字就没了）。

正确做法是往**渲染子窗口** `Chrome_RenderWidgetHostHWND` 发 `WM_CHAR`：
这是真实的字符输入路径，Chromium 当作人打字处理，`input` 事件会触发，React 同步。

最后核对下来，几条路各自的能力边界（全部实测）：

| 方式 | 能插字 | 能删字 | React 同步 |
|---|---|---|---|
| `ValuePattern.SetValue` | ✅ | ❌ | ❌ |
| `WM_CHAR` → 渲染子窗口 | ✅ | ❌ | ✅ |
| `SendInput` / `WM_KEYDOWN` | ❌ | ❌ | — |
| `TextPattern2.InsertText` | Chromium 不提供 | — | — |

所以：**插入用 `WM_CHAR`，删除用"选中全部 + 覆盖"**（UIA `TextRange.Select()`，
之后插入的字符会替换选区）。退格删字做不到——Chromium 删字符要键盘事件，而键盘事件被拦。

**⑧ `WM_CHAR` 有三个必须同时满足的前提，缺一个就"假成功"。**

1. **必须发给渲染子窗口**（`Chrome_RenderWidgetHostHWND`），发给主窗口无效。
2. **输入框必须有键盘焦点**。没有焦点时 `PostMessage` 照样返回 1，
   但字一个都进不去、值一点不变。用 UIA `SetFocus()` 让它聚焦——虽然文档说
   `SetFocus` 只设无障碍焦点，但 Chromium 响应它，之后 `HasKeyboardFocus` 为 True。
3. **空输入框的 `ValuePattern.Value` 返回的是 placeholder 文本**
   （`今天帮你做些什么？ @ 引用对话文件…`），不是空串。所以"判空"必须排除 placeholder，
   否则发出去之后会被判成"文字还在"，或者反过来把"空发送"判成成功。

**⑨ 窗口被遮挡时，Chromium 会销毁整棵无障碍树。**
`render_hwnd` 变 0、UIA 一个 `EditControl` 都读不到。实测切前台后 0.5s 就恢复。
所以注入前必须 `ensure_ready()`：先切前台，再轮询等树重建，然后**清掉缓存重新扫描**
（缓存里的控件引用指向的是旧窗口，读得到值但 `PostMessage` 打不进去）。
副作用是会把你正在看的窗口顶掉——说话本来就是为了发给 WorkBuddy，这个可接受。

**⑩ 麦克风增益必须给到 30dB，默认的 15dB 等于没录到。**

| 增益 | rms | peak | 识别 |
|---|---|---|---|
| 15dB | 25 | 207 | ❌ 空 |
| 18dB | 49 | 408 | ❌ 空 |
| 21dB | 44 | 806 | ❌ 空 |
| 24dB | 71 | 858 | ✅ 100% |
| 27dB | 96 | 820 | ⚠ 80% |
| **30dB** | **278** | **6161** | ✅ **100%** |

30dB 时 peak 6161 离削顶（32767）还有 5 倍余量，安全。
另外**识别前必须做语音增强**（去直流 + 90Hz 高通 + 归一化）：
这块板子的 ES8311 采集带 ≈ -0.84 FS 直流偏置，信号又弱，
不处理直接喂识别器，结果会是"啊男是打工大师"这种纯幻觉。

## 现在的结构

```
        WorkBuddy
      ┌─────┴─────┐
   hooks         MCP
  (Node,0.3s)  (Python)
      └─────┬─────┘
            │ TCP 127.0.0.1:37821   ← 串口只有一个持有者
       ┌────▼─────┐
       │ server   │  devlink.server：独占 COM4、路由、审批、录音
       └────┬─────┘
            │ USB-CDC  换行 JSON
       ┌────▼─────┐
       │ 设备      │  哑终端：显示被要求显示的，上报被按下的
       └──────────┘
```

`host/devlink/` 分层，每层只依赖下一层，前四层是纯函数、可单测：

| 文件 | 职责 |
|---|---|
| `proto.py` | 消息约定 + 编解码 |
| `codec.py` | IMA ADPCM 编解码、重采样、识别前处理 |
| `link.py` | 串口：读线程、重连、发送锁 |
| `speech.py` | 识别（常驻子进程）+ 合成（8k PCM） |
| `inject.py` | 把文字送进 WorkBuddy 输入框（UIA 写值 + 点发送） |
| `terminal.py` | 终端语义：状态 / 通知 / 审批 / 说话→识别→注入 |
| `server.py` | 单实例服务 + 本机控制口 |
| `cli.py` | 命令行与自检 |

入口：`host/server.py`（服务）、`host/hook.js`（hook，Node）、`host/mcp_server.py`（MCP）、
`host/start.cmd`（双击启动）。

## 按键

| 操作 | 设备 | WorkBuddy |
|---|---|---|
| **按住 OK > 0.45 秒** | 采音，屏上"聆听中…"+电平条 | 识别 → 写进输入框 → 点发送 |
| 点一下 OK（< 0.45 秒） | — | 回车 |
| ↑ ↓ | 翻看设备上的对话记录 | 同方向键 |
| 审批时 ↑↓+OK | 本地选"确认/拒绝" | 决定这次工具调用放不放行 |

### "按 OK 没反应"的两个原因（都修了）

1. **通知层挡住了 OK。** `Stop` hook 每轮结束会推一条通知到设备；
   固件原来遇到通知层就直接 `return`（只关通知，不录音）。
   结果你最想说话的时刻（我刚答完），按 OK 只是在关通知。
   → 改成：OK 先关通知，然后**继续走录音流程**。
2. **按住时长要 > 450ms。** 短按被当作"回车"，会 `cancel_talk()`。
   想说话就按住不放，说完松手。

另外：设备正在播报时按 OK 是"停止播报"，播报停了再按一次才开始录音。

## 跑起来

```powershell
# 服务（已装开机自启；也可双击 host\start.cmd）
C:\Users\46755\WorkBuddy\ai-passport\host\.venv\Scripts\python.exe -m devlink.server

# 体检
... -m devlink.cli doctor
# 注入自检：定位输入框 → 写值 → 点发送 → 校验框已空
... -m devlink.cli inject "这条应该自己发出去"
# 字形自检：推一屏中文，肉眼确认没有方框
... -m devlink.cli screen
# 让设备说话 / 推一行字 / 弹审批
... -m devlink.cli say "报告已生成"
... -m devlink.cli reply "这一行"
... -m devlink.cli ask "继续吗" 30
# 麦克风体检：扫增益、报电平和判定
... -m devlink.cli micprobe
```

单测：`python -m tests.test_codec`（5 组，全过）

**hooks 要重启 WorkBuddy 才加载。** 已注册：SessionStart / UserPromptSubmit /
Notification / Stop / PreToolUse。

`PreToolUse`（物理审批）默认关闭，用开关文件启用，否则每个写操作都阻塞等人按键：

```powershell
ni C:\Users\46755\WorkBuddy\ai-passport\host\.approval_on   # 开
rm C:\Users\46755\WorkBuddy\ai-passport\host\.approval_on   # 关
```

超时**不擅自放行**：返回 `ask`，交回 WorkBuddy 自己的权限弹窗。

## 实测到哪一步

| 项目 | 结果 | 依据 |
|---|---|---|
| 中文字库 | ✅ 8058 码点，界面词零缺字 | `tools/check_glyphs.py` |
| 编译 / 烧录 | ✅ app 1.56MB / 8MB | hash 校验通过 |
| 串口协议 | ✅ ping 回 `fw=wb-2.0` | 版本号随 pong 上送 |
| 屏显 / 通知 / 播报 | ✅ 指令全部下发成功 | 你已确认中文正常显示 |
| 上行带宽 | ✅ 11.4 KB/s ≈ 1.0× 实时 | `tools/measure_uplink.py` |
| 麦克风 | ✅ 有信号（15dB 时 peak 200~2000） | 固件记住增益后才稳定 |
| 语音识别 | ✅ **逐字正确** | 4 句里 3 句全对，剩 1 句只是 pdf 大小写 |
| 文字写进输入框 | ✅ 语音原文原样出现在输入框 | ValuePattern 读回校验 |
| 点"发送"键 | ✅ **已自动发出** | 输入框回到 placeholder |
| 输入方式 | ✅ `WM_CHAR` → 渲染子窗口 + SetFocus | 输入后文字保留、不被 React 覆盖 |
| 覆盖/替换 | ✅ 选中全部 + 输入覆盖 | 整段替换成功 |
| 静音裁剪 | ✅ 10.2s → 6.8s，一个字不丢 | `trim_silence`，耗时 0.00s |
| hook 事件送达 | ✅ 6 类全部到位 | `← clear` `← reply` `← notify` … |
| 工具审批 | ✅ 超时→ask、只读→allow、拒绝→deny | 三条分支都测了 |
| 语音识别 | ✅ **信号够强时逐字正确**（rms 2655 → 10 个"测试"全对） | 见下：瓶颈是信号强度 |
| 电平自检 | ✅ 削顶自动降增益、静音跳过不产生幻觉 | `terminal._audio_health` |
| 待发队列清理 | ✅ 4 条幽灵消息全部删除 | `tools/purge_queue.py` |

### 语音识别：引擎没问题，瓶颈是信号强度（已实测确认）

用 PC 音箱对着设备播报，同一套参数三种结果，界限非常清楚：

| 录音条件 | rms | 识别 |
|---|---|---|
| 音箱大声播放 | **2655** | ✅ **逐字全对**（10 个"测试"一个不差） |
| 尾巴 / 弱信号 | 526 | ✅ 也对 |
| 只有底噪 | 164 | 跳过（不产生幻觉） |

**所以识别引擎本身是准的。** 之前失败的那些，rms 只有 229——就是底噪水平，
设备根本没听到声音。

### 真实语音链路已经跑通（2026-10-21 21:02 首次完整走通）

按住 OK 说话 → 麦克风上行 → 识别 → 注入 → 自动发送，全程无需打字。
日志里能完整看到这条链（`cli log`）：

```
21:02:23 按键 ok/press
21:02:25 按键 ok/release
21:02:25 收到语音：44800 采样（2.8s，56 块）
21:02:25 已切掉开头瞬态 78ms
21:02:25 电平自检：ok 削顶=0‰ rms=1144 增益=30dB
21:02:39 ← reply 你：……
```

`rms=1144` 属于可用区间（远距离音箱只有 356，大声播放 2655）。
**每次真实语音都会存到 `host/recordings/last_voice.wav`**，
用 `cli play voice` 可以回放——判断"没听清"还是"说错了"，不用再靠猜。

### 引擎选型：默认 SenseVoice（`WB_ASR_BACKEND=sense`）

同一批音频 A/B：

| 音频 | Paraformer-small | SenseVoice |
|---|---|---|
| 合成·清晰 | 帮我把这周的实验数据整理成表格 | 同（多句号） |
| 合成·数字 | 明天上午**十**点提醒我… | 明天上午**10**点提醒我…（ITN） |
| 合成·专业 | 第三组**副**孔偏差…（无标点） | 第三组**腹**孔偏差…**，**建议…**。** |
| 真实弱信号 rms=356 | 问过他然后就是帮我把持推个坚持 pk | 见过他，然后就是坚持一体推进，坚持一体推进。 |

清晰音频两者都对；弱信号下 SenseVoice 更连贯，且自带标点与数字归一化。
代价：模型 228MB（int8）、首次载入 2.2s，之后每句 0.4s。
Paraformer 保留作为兜底（`WB_ASR_BACKEND=sherpa`）。

> 装模型时的两个坑：并发下载**必须分块校验长度**（某个 Range 403 会留下空洞，
> 文件大小对但模型 `Protobuf parsing failed`）；`tokens.txt` 出现全 NUL 时，
> decode 会报 `invalid unordered_map<K, T> key`——`_sense()` 里已加词表大小校验。

现在服务会做**电平自检**（`terminal._audio_health`）：
- `bad_clip`（削顶 ≥25‰）→ 自动降 3dB 增益，屏上提示"声音太大，请重说"，
  **不把严重削顶的音频喂给识别器**
- `clip`（≥3‰）→ 仍送识别（实测 2‰ 时逐字全对），只顺手降一档
- `silent`（rms < 400）→ 直接提示"没听到声音"，**不识别**
  （引擎在近空输入上会编句子——实测 rms=169 时它照样输出
  "测试测试测试…"，看着像成功其实是幻觉）
- `ok` → 正常识别

阈值依据：30dB 下底噪实测 rms ≈ 180~230，真实近距离说话 rms ≈ 2500。
`SILENT_RMS=400` 卡在中间。

### 每次开录的 100ms 里藏着一段爆音（会让识别结果开头多几个乱字）

放大看录音头 100ms：

```
0.0s rms=23077 clip=464   ← codec 上电爆音，占满 464 个样本
0.1s rms= 3144 clip=  0
0.2s 之后 rms=120~650     ← 真实环境声
```

这段不是人声，却造成两个后果：
1. `peak` 直接顶到 32768，把"削顶"判据打满 → 误报"声音太大"，白降增益
2. 被当成语音开头送进识别器 → 结果开头多出几个莫名其妙的字

修法：`codec.trim_transient()`，只切开头（最后一个满幅样本 +20ms），
中间一个样本都不动。实测切完 `peak` 从 32768 降到 15240。

### 事件日志是持久的（`cli log` 能直接看）

以前日志靠 shell 的 `> srv.log`，**一重启历史就没了**——
于是"用户到底按没按 OK"这种问题，重启一次证据就消失，今天正是因此反复误判。

现在 `log()` 同时追加到 `host/events.log`（超 2MB 轮转一个 `.1`），
并且固件在**按下 OK 时上报事件**：

```
← log  # 看最近 40 条
20:49:52 ← record
20:49:52 音频开始上行
20:49:56 收到语音：64000 采样（4.0s，80 块）
```

有了这三行就能一眼分辨：
- **没有 `按键 ok/press`** → 按键没触发（被弹窗挡住 / 按得太短）
- **有 press 但没有 `音频开始上行`** → 麦克风没采到（codec/增益问题）
- **有音频但识别乱** → 信号太弱或环境太吵

### 每轮结束把结论念出来（`speak_last`）

之前播报能力写好了却**从没被用过**（`events.log` 里 `播报已推送` 是 0 次）——
因为没有任何地方知道"AI 刚说了什么"。`.env` 里的 `WB_SPEAK_REPLY=1` 是个**死配置**，
没有任何代码读它。

现在接通了。关键发现：**Stop hook 的 payload 带 `transcript_path`**，
指向会话的 jsonl 对话记录，从里面能取到最后一条 assistant 回复。

```
WorkBuddy 一轮结束
  → Stop hook（Node，234ms 返回）
  → 发 op=speak_last + transcript_path
  → 服务侧 devlink/digest.py 从 jsonl 尾部倒找最后一条 assistant message
  → 提取 output_text → 清 markdown → 截断 → 推流到扬声器
```

**三个设计要点**（都是为了不拖慢 WorkBuddy）：

1. **不 await**：一次播报 7~25 秒，而 Stop hook 的 timeout 只有 8 秒。
   服务把它丢进后台 task，立刻回包。
2. **只读文件尾部 600KB**：transcript 会长到十几 MB（本会话实测 18MB），全文读一遍既慢又浪费。
3. **播报串行化**：两轮挨得近时两个播报并发推流会把设备环形缓冲写乱，加锁。

**内容是怎么选的**：清掉 markdown 后截到 120 字（中文 TTS 约 4~5 字/秒，约 25 秒）。
表格和代码块**整段跳过**——念"竖线、竖线、减号"毫无意义。

想控制念什么，在回复里写一行：

```markdown
> 🎧 语音摘要：语音三次识别全对，播报已接通。
```

有这行就只念它（上限放宽到 200 字）；没有才退回"清理后开头 120 字"。
后者常念到"我先看一下代码"这种零信息量的开场，所以显式标记值得写。

**开关**：`WB_SPEAK_REPLY=0` 关掉（默认开）。
**手动验音质**：

```powershell
& $V -m devlink.cli speaklast --dry   # 只打印会念什么，不出声
& $V -m devlink.cli speaklast         # 真的念
```

实测：`56872 采样 / 7.1s / 56 块`，hook 侧耗时 234ms。

### 诊断工具（都不碰 WorkBuddy 输入框）

```powershell
$V = "C:\Users\46755\WorkBuddy\ai-passport\host\.venv\Scripts\python.exe"
$T = "C:\Users\46755\WorkBuddy\ai-passport\tools"

& $V -m devlink.cli log 40         # 最近 40 条设备事件（按键/录音/识别/注入）
& $V -m devlink.cli rec 5          # 录音+识别+存盘，打印电平
& $V -m devlink.cli play raw       # 回放原始录音（判断"没录到"还是"识别不准"）
& $V -m devlink.cli play clean     # 回放增强后
& $V -m devlink.cli play voice     # 回放上一次**真实语音**（按住 OK 说的那句）
& $V $T\calib_gain.py              # 自动校准增益（找够响但不削顶的档位）
& $V $T\diag_level.py              # 底噪 vs 说话，算信噪比并给判定
& $V $T\analyze_clip.py            # 逐 100ms 看 rms 和削顶分布
& $V $T\loopback.py                # 受控回环：PC 朗读固定句 → 设备录 → 识别比对
& $V $T\inspect_queue.py           # 只读检查待发队列里有没有残留
& $V $T\purge_queue.py             # 清 WorkBuddy 待发队列里的幽灵消息
```

### 发送这一步的真实约束

WorkBuddy **正在回话时，发送键不存在**——输入框右下角那个位置会变成"停止"。
所以用户在我说话时按 OK 说话，文字会进输入框，但那一刻发不出去。

服务对此的处理：写进去之后后台每 2 秒看一次，**一旦"停止"消失就点发送**；
**120 秒还等不到，就把输入框清空**并提示"没发出去，已清空，请重说一次"。

为什么超时要清空而不是保留：保留的话，用户之后随手按一下回车，
这条"过期的话"就会被当成新消息发出去——实测发生过，
我自己的测试残留就这样被发进了对话。丢一句待发内容比发幽灵消息好。

> **教训**：任何时候都别在用户真实的输入框里留测试文本。
> 注入自检要用记事本当目标，或者写完立刻清掉。

### 清空输入框：只能用 `WM_CHAR`，不能用 `SetValue`（幽灵消息的根）

`ValuePattern.SetValue("")` 只清 DOM，**React state 里仍留着整句话**。
任何一次重渲染，React 都会把旧文字写回输入框——用户看到有字就按回车，
于是同一条过期消息被反复发出去（实测连发了好几次同一句）。

`WM_CHAR` 会触发 `input` 事件，React state 才跟着变。但两个限制：
- 退格删不掉：`WM_CHAR 0x08` 和伪造的 `WM_KEYDOWN` 都是无效的（有焦点也一样），
  Chromium 只接受字符消息
- 所以"清空"的实际做法是 **选中全部 + 输入一个空格**

留一个空格是刻意的：WorkBuddy 把纯空白视为空内容（发送键不出现），按回车发不出去。
而且 React state 已被同步成 `" "`，旧句子不会再被渲染回来。残留因此完全无害。

对应实现：`inject.clear_soft()`。**`clear_hard()`（SetValue）不要单独用。**

### 两个清的基础区分

| 命令 | 清的是什么 |
|---|---|
| `cli clear` | **设备屏幕上**的文字 |
| `cli purge` | **WorkBuddy 输入框**里的当前内容（走 WM_CHAR，React state 同步） |

### 幽灵消息的真因：WorkBuddy 有个"待发队列"

不是 React 回填，**是队列**。

WorkBuddy 输入框上方有个待发送队列（每项右侧有 转发 / 编辑 / 删除 三个按钮）。
我早先做注入测试时，文字被反复写进输入框并触发发送，WorkBuddy 把它们
**排进了待发队列**。而队列会在**每一轮 AI 回答结束、界面空闲时自动排出一条发出去**——
这就是为什么那几句测试文本会在不同时间点自己出现，日志里却完全没有 `← talk`/`收到语音`。

诊断特征（记住这三条就能立刻认出）：
1. 日志有 `← reply 你：<那句话>`，但**没有** `← talk`、**没有** `收到语音`
2. 出现时机总在 `← status 这一轮做完了` 之后 2~3 秒
3. 内容是历史测试文本，不是用户刚说的话

处理和工具：`tools/purge_queue.py` —— 遍历队列，点每项最右边那个按钮（垃圾桶）删除，
循环到队列空。按钮没有 Name，靠"输入框上方 + x 在 900~1100 + 每行最右"定位。

| 命令/脚本 | 清的是什么 |
|---|---|
| `cli clear` | 设备屏幕上的文字 |
| `cli purge` | WorkBuddy 输入框里的当前内容 |
| `tools/purge_queue.py` | **WorkBuddy 待发队列里的排队消息** |

**教训（升级版）**：注入测试绝不能打在用户真实的 composer 上。
即使之后清了输入框，已经进队列的消息还会一个个自己发出去。
测试要用记事本当靶子，或者用一个不接发的临时窗口。

## 已知限制

- 识别后端默认 Paraformer（离线、本地跑）。模型在 `host/models/`，约 78MB。
- 本机 `ctranslate2` / `torch` 原生库加载即崩（0xC0000005），所以 Whisper 不可用；
  识别跑在常驻子进程里，崩了不影响服务。
- 串口上行约 11KB/s，超过 ~15s 的连续语音会明显滞后。
- 屏 240×320 无 PSRAM，只做纯文本 + 色块。
- 注入依赖 WorkBuddy 的 Chromium 无障碍树：如果哪天它换框架或关掉 UIA，
  `cli inject` 会直接报"定位不到输入框"，到时再调。

## 目录

```
main/fonts/lv_font_wb_zh_16.c   ← tools/gen_font.py 生成（GB2312 全集，16px 4bpp）
tools/gen_font.py               生成字库（系统黑体 → LVGL）
tools/check_glyphs.py           校验字形覆盖（加字前先跑）
tools/measure_uplink.py         测上行吞吐
tools/dl_paraformer.py          下载识别模型（多线程、可续传）
tools/bench_hook.py             hook 延迟对比
host/devlink/                   主机程序（见上表）
host/hook.js                    hook 转发器（Node，比 Python 快 3 倍）
host/start.cmd                  双击启动服务
```
