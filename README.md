# AI PASSPORT × WorkBuddy

把一块 ¥119 的电子工牌做成 WorkBuddy 的**物理终端**。

一块 ESP32-C3 小屏硬件，插在电脑上：**说话下指令、屏幕看状态、按一下拍板审批、每轮结束把结论念出来**。

设备本身不跑 AI——识别、推理、合成都留在电脑侧。它只负责四件硬件该干的事：
采集声音、显示画面、上报按键、播放声音。

> 本项目是 [FoloToy/AI-PASSPORT](https://gitee.com/FoloToy/ai-passport) 的二次开发，
> 应用层完全重写（开机直接进伴侣页，不走 demo 菜单）。

---

## 它到底解决什么

WorkBuddy 在电脑上跑长任务时，人不在屏幕前。你想要的只有四件事：

| 需求 | 原来 | 现在 |
|---|---|---|
| 想知道跑到哪了 | 回电脑看 | 屏幕自己更新，标题栏显示会话 |
| 想给个新指令 | 走回电脑 | 按住 OK 说话，松开自动发送 |
| 危险操作想拦一下 | 看不见就放过去了 | 弹到设备上，按 OK 才放行 |
| 想知道结论 | 回电脑看 | 每轮结束直接念出来，按 OK 可打断 |

## 硬件

| 项 | 规格 |
|---|---|
| 芯片 | ESP32-C3（RISC-V，无 PSRAM） |
| 屏幕 | 240×320 ST7789 |
| 音频 | ES8311 codec（麦克风 + 扬声器） |
| 按键 | 上 / 下 / OK（ADC 分压，单通道） |
| Flash | 8MB（app 分区 7.9MB，固件占 1.56MB） |
| 电池 | 500mAh（连续联网约 5–6 小时） |
| 连接 | USB-CDC 串口，**一根 USB 数据线搞定，免驱动** |

## 快速开始

### 1. 刷固件（约 1 分钟）

**方式一：网页刷写（推荐，无需装工具）**

1. 按住左上角开机键开机，用 USB-C **数据线**连电脑
2. Chrome / Edge 打开 [ai-passport.folotoy.cn](https://ai-passport.folotoy.cn)
3. 点「安装」→ 选择串口 → 等待完成（设备会自动重启）

**方式二：命令行**

```bash
esptool.py --chip esp32c3 --port COM4 --baud 460800 \
  write_flash --flash_mode dio --flash_size 8MB --flash_freq 80m \
  0x0 build/bootloader/bootloader.bin \
  0x8000 build/partition_table/partition-table.bin \
  0x10000 build/FoloToy-AI-Passport.bin
```

自己编译见 [skills/passport-build](skills/passport-build/)（ESP-IDF v5.5）。

### 2. 装主机程序

```powershell
cd host
python -m venv .venv
.venv\Scripts\python.exe -m pip install -r requirements.txt
copy .env.example .env
.venv\Scripts\python.exe -m devlink.cli doctor   # 体检：会告诉你缺什么
```

语音识别模型约 307MB，`doctor` 会提示下载。首次跑一次就好。

### 3. 接入 WorkBuddy

```powershell
.venv\Scripts\python.exe -m devlink.cli setup      # 写 hooks（5 类）+ MCP 配置
.venv\Scripts\python.exe -m devlink.cli install    # 开机自启（可选）
```

`setup` 的三个安全底线：**改前自动备份**、**幂等**（重复跑不会叠加重复 hook）、
**只合并不覆盖**（你自己的其它 hook 和 MCP server 一个字不动）。
想撤销就 `cli unsetup`。

之后启动后台服务：

```powershell
.venv\Scripts\python.exe server.py     # 常驻；或直接双击 host\start.cmd
```

服务独占串口并监听 `127.0.0.1:37821`，hooks / MCP / 命令行都通过它说话，
所以它必须一直跑着。已在跑时重复启动会提示"已有实例"并退出，不会冲突。

> MCP 新条目需要在 WorkBuddy 的连接器页面点一次「信任」才生效。

**先验证硬件再接入**——顺序反了，一旦失败你分不清是硬件、识别还是注入的问题：

```powershell
.venv\Scripts\python.exe -m devlink.cli screen        # 画一屏中文，看有没有方框
.venv\Scripts\python.exe -m devlink.cli say "测试"     # 让它说话
.venv\Scripts\python.exe -m devlink.cli rec 5         # 只录音识别，不碰输入框
```

## 三种能力，各自的实现

```
按住 OK 说话
  → 16kHz ADPCM 上行（11.4 KB/s，实时）
  → sherpa-onnx + SenseVoice 离线识别（5s 音频约 0.4s）
  → WM_CHAR 发渲染子窗口 → UIA Invoke 点发送

每轮结束念结论
  → Stop hook 拿到 transcript_path
  → 从 jsonl 取最后一条 assistant 回复，剥掉 markdown
  → TTS → 8kHz ADPCM 推流 → 设备扬声器

危险操作拦截
  → PreToolUse hook 阻塞等待
  → 设备弹框，按 OK / ↓
  → 结果回传成 permissionDecision
```

## 三个不该被忽略的发现

**① 语音输出现已真的可用。** 实测三次同一段录音，识别结果完全一致、逐字正确
（"接下来我们可以做什么？" 11 个字一字不差）。

**② 播报曾经是个死功能。** 代码写好了、也测过能出声，但 `WB_SPEAK_REPLY=1`
这个开关**没有任何代码读它**——从没在真实路径上用过。现在接在 Stop hook 上。

**③ 多会话不需要"选择发到哪"。** WorkBuddy 是单窗口多标签，所有会话共用
**同一个输入框和同一个渲染子窗口**，注入天然落在你眼前那个标签上。
真问题反过来：所有会话的事件挤到同一块屏，看不出谁是谁。
现在靠 ①②③ 标签区分，单会话时零前缀。

## 项目结构

```
main/                    固件应用层（LVGL UI、状态机、按键、播报）
components/wb_protocol/  协议实现（换行分隔 JSON）
components/bsp/          板级驱动
host/devlink/            主机侧：链路 / 协议 / 语义 / 接口
host/hook.js             WorkBuddy hook 转发器（特意用 Node，冷启动 0.28s）
tools/                   检查与验证工具
docs/                    设计文档与踩坑记录
skills/                  给 AI agent 的技能包（构建/烧录/调试/测试）
```

## 验证工具

这个项目最值钱的部分不是功能，是**怎么确认功能真的生效**。

```bash
python tools/check_font_cov.py ①②③    # 字库到底覆盖哪些码点（解析 LVGL cmap）
python tools/sim_screen.py             # 1:1 复现固件折行，算出屏幕每行内容
python tools/probe_fw_log.py COM4      # 独占串口发指令 + 读固件日志，末端取证
python tools/bench_relay.py            # 把串口换 TCP，测公网延迟下协议是否还成立
python tools/push_to_github.py --check # 推送前体检：SSH/公钥/仓库/token 各查一遍
```

写这些是因为踩够了「返回成功但什么都没发生」：`SendInput` 少 8 字节静默返回 0、
`PostMessage` 返回 1 字没进去、`SetValue` 改了 DOM 但 React 不认、
hook 在 `connect` 就 exit 导致**一个字节都没发出去**。

## 已知限制（写清楚，免得你踩）

- **注入会抢前台。** 窗口被遮挡时 Chromium 会销毁整棵无障碍树，必须切回来重建。
- **退格无效**，清空只能"选中全部 + 覆盖"。
- **电池撑不过一整天。** WiFi 常连约 5–6 小时，适合"按需连接"而非常在线。
- **本地 Whisper 在部分机器上载入即崩**（ctranslate2 / torch DLL）。默认走 SenseVoice。
- **PreToolUse 审批默认关闭**，建 `host\.approval\.on` 才开。它现在"拦所有非只读工具"，
  粒度偏粗，还没做成"只拦危险操作"。
- **依赖 Chromium 的 DOM 结构**，WorkBuddy 大改版可能让注入失效。

## 相关

- 设计文档与十个根因：[README_WORKBUDDY.md](README_WORKBUDDY.md)
- 主机侧细节：[host/README.md](host/README.md)
- 资产清单：[ASSETS.md](ASSETS.md)
- 上游项目：[FoloToy/AI-PASSPORT](https://gitee.com/FoloToy/ai-passport)

## License

跟随上游仓库。二次开发部分见 [LICENSE](LICENSE)。
