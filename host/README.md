# WorkBuddy 硬件伴侣 · PC 主机程序

AI PASSPORT（ESP32-C3 + 240×320 屏 + 麦克风 + 扬声器 + 三个按键）通过**板载 USB 线**
连电脑，充当 WorkBuddy 的物理遥控器：说话下指令、看状态、拍板审批、听播报。

设备本身**不跑** STT/LLM/TTS——所有智能都在 PC 侧。设备只做四件事：
采集音频、显示画面、按键上报、放声音。

```
┌──────────── AI PASSPORT ────────────┐        ┌──────────── PC ─────────────────┐
│ 麦克风(ES8311) ─┐                   │        │                                 │
│ 屏幕(ST7789)  ◄─┼── 新行 JSON ──────┼─USB──► │  devlink/server.py              │
│ 扬声器(ES8311)◄─┘   USB-CDC          │ CDC    │   ├ speech.py    SenseVoice 识别 │
│ 三键(ADC 分压) ────────────────────┼───────►│   ├ inject.py    WM_CHAR 注入     │
└─────────────────────────────────────┘        │   ├ speech.py    TTS → ADPCM 下发 │
                                               │   └ digest.py    抓回复给播报     │
                                               └──────────┬──────────────────────┘
                                                          │ hooks（5 类事件）
                                                   ┌──────▼──────┐
                                                   │  WorkBuddy  │
                                                   └─────────────┘
```

## 三层结构

| 层 | 文件 | 职责 |
|---|---|---|
| 传输 | `devlink/link.py` | 独占串口，收发按行切分 |
| 协议 | `devlink/proto.py` / `components/wb_protocol` | 换行分隔 JSON，纯函数无判断 |
| 语义 | `devlink/terminal.py` | 按键/音频 → 该做什么（唯一有产品逻辑的地方） |
| 接口 | `server.py` / `cli.py` / `mcp_server.py` | TCP 控制口、命令行、MCP 工具 |

## 安装

```powershell
cd host
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt
copy .env.example .env          # 按需改串口号
```

语音识别模型约 **307MB**，首次运行单独下载（不进版本库）：

```powershell
.venv\Scripts\python.exe -m devlink.cli doctor
```

`doctor` 会检查串口、固件、服务、依赖、模型是否齐备，并告诉你缺哪一步。

## 先自检，再接 WorkBuddy

```powershell
$V = "host\.venv\Scripts\python.exe"

& $V -m devlink.cli doctor                  # 体检
& $V -m devlink.cli screen                  # 画一屏中文，检查字形有没有方框
& $V -m devlink.cli say "语音链路测试"        # 让设备说话
& $V -m devlink.cli rec 5                   # 只录音+识别，不碰输入框（诊断用）
& $V -m devlink.cli play raw                # 回放上次录音，判断是"没录到"还是"识别不准"
```

**顺序很重要**：先确认设备能出声、能认字，再去碰 WorkBuddy 输入框。
反过来做，一旦失败你分不清是硬件、识别还是注入的问题。

## 交互约定

| 操作 | 设备行为 | PC 行为 |
|------|---------|---------|
| **按住 OK**（>0.45s） | 采音，屏幕显示"聆听中"与电平条 | 识别成文字 → 注入 WorkBuddy 输入框并回车 |
| **点一下 OK** | 无 | 发 `Enter` |
| **↑ / ↓** | 翻看设备上的对话记录 | 同时发方向键 |
| 审批弹框时 **↑↓ + OK** | 本地选"确认/拒绝" | 结果经 `wb_answer` 回到等你决策的那次调用 |
| 播报中 **按 OK** | 立刻打断 | 丢弃剩余音频缓冲 |
| 收到通知 | 卡片 + "叮"一声 | — |

## 命令

```powershell
& $V -m devlink.cli say "报告已生成"          # 让设备说话
& $V -m devlink.cli type "查一下昨天数据"      # 直接注入并发送
& $V -m devlink.cli status thinking "跑着呢"
& $V -m devlink.cli reply "一行字"
& $V -m devlink.cli sessions                # 多会话时 ①②③ 分别是哪个会话
& $V -m devlink.cli speaklast --dry         # 预览"这一轮会念什么"，不出声
& $V -m devlink.cli log 40                  # 最近 40 条事件
```

| 命令 | 作用 |
|---|---|
| `doctor` | 体检：串口/固件/服务/依赖/模型 |
| `screen` | 画一屏中文，检查字形 |
| `say` | 让设备播报 |
| `status` / `reply` / `clear` | 推状态 / 追加对话 / **清设备屏幕** |
| `purge` | 清 **WorkBuddy 输入框**残留（会同步 React 状态） |
| `type` | 注入文字并按需回车 |
| `mic` / `rec` / `play` | 真实走一遍语音 / 只识别 / 回放录音 |
| `micprobe` / `gain` | 电平诊断 / 调麦克风增益 |
| `sessions` / `forget` | 查多会话标签 / 清空标签表 |
| `speaklast [--dry]` | 手动触发"念本轮结论" |
| `setup` / `unsetup` | 写入 / 撤销 WorkBuddy hooks 与 MCP 配置（备份+幂等+只合并） |
| `install` / `uninstall` | 写入 / 移除开机自启 |

⚠ `clear` 和 `purge` 不是一回事：`clear` 清屏幕，`purge` 清输入框。
这两个搞混会浪费很多时间。

## MCP 工具

由 `mcp_server.py` 经 stdio 提供，本进程不碰串口，只转发给后台服务。

| 工具 | 作用 |
|------|------|
| `wb_ping()` | 链路探测 |
| `wb_status(state, text)` | 状态药丸：idle / thinking / waiting / alert |
| `wb_reply(text, speak=False)` | 文字上屏（可选同时朗读） |
| `wb_speak(text)` | 只用扬声器播报 |
| `wb_notify(title, body, priority)` | 通知卡片 + 提示音 |
| `wb_ask(text, timeout_sec)` | 弹审批框，返回 confirm / cancel / timeout |
| `wb_listen(timeout_sec)` | 等用户按住设备说一句，返回文字 |
| `wb_type(text, submit)` | 注入输入框 |
| `wb_last_heard()` / `wb_clear()` | 最近识别结果 / 清屏 |

配置（`cli install` 会自动写）：

```json
"ai-passport-companion": {
  "command": "<repo>/host/.venv/Scripts/python.exe",
  "args": ["<repo>/host/mcp_server.py"],
  "env": { "WB_DAEMON_PORT": "37821", "WB_ASR_BACKEND": "sense" }
}
```

## 环境变量

完整说明见 `.env.example`，这里只列要点。

| 变量 | 说明 | 默认 |
|------|------|------|
| `WB_PORT` | 串口，留空自动探测 VID:PID=303A:1001 | 自动 |
| `WB_ASR_BACKEND` | `sense` / `sherpa` / `sapi` / `whisper` | `sense` |
| `WB_MIC_GAIN` | 麦克风增益 dB，**这块板子必须 ≥25** | `30` |
| `WB_FOCUS` | 注入前把 WorkBuddy 切前台 | `1` |
| `WB_ASK_TIMEOUT` | 审批等待秒数 | `90` |
| `WB_SPEAK_REPLY` | 每轮结束是否朗读结论 | `1` |
| `WB_SAPI_VOICE` | 播报音色关键字 | 第一个中文音色 |

## 协议

换行分隔 JSON，握手细节见 `components/wb_protocol/include/wb_protocol.h`。

- **设备 → PC**：`wb_audio{s,b}`（ADPCM，base64）、`wb_audio_end`、`wb_key_event{button,event}`、`wb_answer{id,v}`、`wb_pong`
- **PC → 设备**：`wb_status{state,text}`、`wb_header{text}`、`wb_reply{text}`、`wb_notification`、`wb_ask`、`wb_show_options`、`wb_clear_screen`、`wb_ping`、`wb_heartbeat`、`wb_tts_begin{hz,a}`、`wb_tts_chunk{s,b}`、`wb_tts_end`、`wb_tts_stop`、`wb_debug_talk{on}`、`wb_mic_gain{db}`

音频一律 IMA ADPCM：上行 16 kHz、下行 8 kHz，每块自带预测器头，
丢块不会污染后续解码。

固件对**不认识的 type 只打一行 debug 日志就丢掉**，所以主机侧可以比固件新。

## 多会话

WorkBuddy 是**单窗口多标签**，所有会话共用同一个输入框，hook 事件也挤到同一块屏。
2 个以上会话活跃时，服务给每个会话分配 ①②③ 并标在消息前，标题栏显示 `WB① 2会话`。

- 单会话时**不加任何前缀**，界面和以前一样
- 标签按首次出现顺序分配，不随活跃度重排
- 30 分钟无活动释放标签
- 从单会话切到多会话时会**清屏**——历史行的归属已无从判断，宁可丢历史也不标错

## 排障

| 现象 | 处理 |
|------|------|
| 打开串口失败 | 确认是数据线；`MODE` 灯亮但无串口 = 充电线。或后台服务正占着串口 |
| 设备屏幕红点 | PC 侧服务没在跑，或 30s 无消息（固件有链路活性判定） |
| 识别不准 / 返回空 | **先看电平**：`cli rec 5` 看 rms，低于 500 就是没录到，调 `WB_MIC_GAIN` |
| 播报没声 | `cli say "测试"`；`WB_SAPI_VOICE` 指定中文音色 |
| 按键没反应 | WorkBuddy 需在前台（`WB_FOCUS=1` 会自动切） |
| 字发不出去 | `WM_CHAR` 必须发给渲染子窗口；窗口被遮挡时 Chromium 会销毁无障碍树 |

## 已踩过的坑（改代码前值得读）

1. LVGL 内置中文字库名字像覆盖常用字，实际只有 1456 码点 → 满屏方框。
   现在用自建 8058 码点字库，`tools/check_font_cov.py` 可验证。
2. `SendInput` 的 `INPUT` 结构少算 8 字节 → 静默返回 0，什么都不发生。
3. `usb_serial_jtag_write_bytes` 会部分写入 → 每条消息尾部被截断，必须循环写。
4. hook 在 `connect` 回调里就 `exit` → 数据一个字节都没发出去。必须等 `close`。
5. `SetValue` 改 DOM 不触发 `input` 事件 → 字看得见但发不出、删不掉。
6. 空输入框的 `ValuePattern.Value` 返回 placeholder 而不是空串。
7. `clear` 清屏幕，`purge` 清输入框，搞混好几轮。

这些的共同点是**看着成功、实际没生效**。验证时一定要读回目标状态，
不要相信返回值。
