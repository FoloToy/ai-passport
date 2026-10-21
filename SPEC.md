# SPEC: WorkBuddy × FoloToy AI-Passport 集成

> 日期：2026-10-20 | 状态：**待开发** | 负责人：Vito

---

## 1. 目标

将 FoloToy AI-Passport（ESP32-C3）作为 WorkBuddy 的物理交互终端：
- **语音输入**：长按硬件录音键 → BLE 传输音频 → PC Whisper ASR → 注入 WorkBuddy 输入框
- **弹窗控制**：WorkBuddy 弹窗出现时，硬件 UP/DOWN/OK 按钮 → PC 模拟键盘事件
- **完成通知**：WorkBuddy 任务完成后 → BLE 推送通知 → 硬件屏幕显示 / 扬声器播报

---

## 2. 系统架构

```
┌─────────────────────────────────────────────────────────────┐
│  ESP32-C3 (FoloPassport)     PC Bridge (Python)            │
│  ┌──────────────────┐      ┌──────────────────────────┐   │
│  │ BLE GATT Server   │ BLE  │ BLE Client (bleak)       │   │
│  │ (NUS, 512 MTU)   │←────→│                          │   │
│  │                  │      │ wb_protocol parser        │   │
│  │ wb_protocol      │      │                          │   │
│  │  - voice chunks  │      │ Voice pipeline:          │   │
│  │  - options UI    │      │   hex → PCM → Whisper    │   │
│  │  - notifications │      │   → inject to WorkBuddy   │   │
│  │  - keyboard evts │      │                          │   │
│  │                  │      │ Keyboard pipeline:        │   │
│  │ ES8311 (音频)    │      │   UP/DOWN/OK → pyautogui │   │
│  │ ST7789 (屏幕)    │      │                          │   │
│  │ 3-button        │      │ WorkBuddy integration:    │   │
│  └──────────────────┘      │   - clipboard 注入文本    │   │
│                            │   - keyboard 模拟         │   │
│                            │   - output 监控 (VTC MCP?)│   │
│                            └──────────┬───────────────┘   │
└───────────────────────────────────────┼───────────────────┘
                                        │ keyboard / clipboard
                                        ↓
                              ┌──────────────────────┐
                              │  WorkBuddy (本地)      │
                              │  输入框 / 弹窗 / 输出  │
                              └──────────────────────┘
```

---

## 3. 固件端改动（ESP32-C3）

### 3.1 新增协议消息

| 方向 | 消息类型 | 用途 |
|------|---------|------|
| Device→PC | `wb_key_event` | 硬件按键事件（UP/DOWN/OK press/release/hold） |
| PC→Device | `wb_tts_play` | PC 请求设备 TTS 播报文字 |
| PC→Device | `wb_clear_screen` | PC 请求清屏 |
| PC→Device | `wb_key_acked` | PC 确认键盘事件已处理 |

### 3.2 新增 UI 页面

**WorkBuddy 状态页（重新设计，不能复用 demo 菜单）：**
- 连接状态指示（绿色=已连接/红色=未连接）
- 当前模式标签：录音中 / 等待响应 / 显示结果 / 通知
- 最近的 WorkBuddy 响应文字（最多2行，超长滚动）
- 硬件按钮状态指示
- 右上角电池电量

**通知页面（模态覆盖层）：**
- 全屏半透明遮罩 + 居中卡片
- 标题 + 正文 + 优先级色条（普通=绿/高=红/低=灰）
- 自动消失（普通5秒，高优先级手动确认）
- 可选 TTS 播报正文

**TTS 播报状态：**
- 播放中显示波形动画
- 停止按钮（OK 键）

### 3.3 音频输出（TTS）

使用 ES8311 播放 PCM 数据：
- 接收 PC 发来的 TTS PCM 数据（BLE 分包）
- 或者：PC 发文字，设备本地用轻量 TTS（如 pico2wave / CSMS）
- 回退：PC 直接发 WAV/PCM hex 数据，设备直接播放

### 3.4 键盘事件转发（PC端控制）

硬件 UP/DOWN/OK 按键 → BLE 发送到 PC Bridge → PC Bridge 转换为 pyautogui 键盘事件：
- UP → `pyautogui.press('up')`
- DOWN → `pyautogui.press('down')`
- OK(短按) → `pyautogui.press('enter')`
- OK(长按) → `pyautogui.press('enter')`（同短按）

### 3.5 UI 重新设计规则（强制）

按照 FoloToy AI-Passport 项目规范：
- **禁止**复用 `main` 测试菜单、demo 页面
- 需设计全新 LVGL 页面
- BSP API（bsp_audio、bsp_display）可复用
- 中文显示需验证字形覆盖

---

## 4. PC Bridge（Python）

### 4.1 依赖

```
bleak>=0.20           # BLE GATT 客户端
openai-whisper         # ASR（本地 GPU）
pyautogui             # 键盘模拟
pyperclip / clipboard  # 剪贴板
```

### 4.2 BLE 连接

- 服务 UUID：`6e400001-b5a3-f393-e0a9-e50e24dcca9e`（NUS）
- TX Characteristic（写）：`6e400002-b5a3-f393-e0a9-e50e24dcca9e`
- RX Characteristic（通知）：`6e400003-b5a3-f393-e0a9-e50e24dcca9e`
- MTU：512
- 设备广播名：`FoloPassport`

### 4.3 消息处理

**收到 `wb_voice_chunk`（hex PCM 分包）：**
1. 累积 hex 数据到缓冲
2. 收齐后转回 PCM bytes
3. 调用 Whisper ASR（本地 GPU）→ 获取文字
4. 通过 clipboard 注入 + pyautogui 发送（模拟粘贴+回车）
5. 等待 WorkBuddy 响应（监控输出）

**收到 `wb_key_event`（UP/DOWN/OK 事件）：**
1. 解析按键类型和事件（press/release/hold）
2. 转换为 pyautogui 键盘事件
3. 执行键盘操作
4. 可选：发送 `wb_key_acked` 回 ESP32

**WorkBuddy 响应检测（定时轮询或事件）：**
1. 轮询剪贴板或监控 WorkBuddy 输出区域
2. 检测到完整响应后，发送 `wb_notification` 回 ESP32
3. 如果响应含选项，发送 `wb_show_options`

**TTS 请求（PC 端触发）：**
1. TTS 引擎（如 edge-tts / pyttsx3）生成音频
2. 通过 BLE 发送 `wb_tts_play` + PCM 数据（或 WAV 路径）
3. ESP32 接收并播放

### 4.4 WorkBuddy 集成方式

**文本注入流程：**
```
ASR文字 → 写入剪贴板 → pyautogui Ctrl+V → pyautogui.press('enter')
```

**弹窗控制流程：**
```
收到 wb_key_event(UP) → pyautogui.press('up')
收到 wb_key_event(OK) → pyautogui.press('enter')
```

**响应检测：**
- 方案A：轮询剪贴板变化，检测 WorkBuddy 输出
- 方案B：监听系统通知（Windows Toast）
- 方案C：通过 VTC MCP 的输出管道

### 4.5 命令行接口

```bash
python wb_bridge.py --device FoloPassport --port COM4
python wb_bridge.py --scan                    # 扫描可用 BLE 设备
python wb_bridge.py --whisper-model base      # 指定 Whisper 模型
python wb_bridge.py --dry-run                 # 不执行键盘操作，只打印日志
```

---

## 5. 实现步骤

### Phase 1: PC Bridge 基础（优先级最高）
1. BLE 连接 + 消息收发（bleak）
2. 语音接收 + Whisper ASR
3. 文本注入 WorkBuddy（clipboard + pyautogui）
4. 基本日志输出

### Phase 2: WorkBuddy 响应检测 + 通知推送
1. 响应检测（轮询剪贴板）
2. `wb_notification` 发送
3. `wb_show_options` 实现

### Phase 3: 硬件键盘事件转发
1. `wb_key_event` 协议支持
2. ESP32 固件添加键盘事件发送
3. PC Bridge 接收并执行 pyautogui

### Phase 4: 固件 UI 增强（Phase 1 并行）
1. 新设计 WorkBuddy 状态页面
2. 通知模态覆盖层
3. TTS 播报支持

### Phase 5: TTS 集成
1. PC 端 TTS 引擎
2. BLE 传输 PCM 数据
3. ESP32 音频播放

---

## 6. 文件清单

### 固件（ESP32-C3）

| 文件 | 改动 |
|------|------|
| `main/demo_workbuddy.c` | 新增：WorkBuddy 集成主页面（LVGL UI） |
| `main/demo_workbuddy.h` | 新增：页面声明 |
| `components/wb_protocol/src/wb_protocol.c` | 新增：`wb_key_event`、`wb_tts_play` 消息处理 |
| `components/wb_protocol/include/wb_protocol.h` | 新增：消息类型枚举 |
| `components/bsp/src/bsp_audio.c` | 确认：TTS 播放支持（bsp_audio_write PCM）|
| `main/CMakeLists.txt` | 注册新 demo 源文件 |

### PC 端（Python）

| 文件 | 用途 |
|------|------|
| `wb_bridge/wb_bridge.py` | 主入口 |
| `wb_bridge/ble_client.py` | BLE 连接和消息收发 |
| `wb_bridge/voice_asr.py` | Whisper ASR 封装 |
| `wb_bridge/keyboard_ctrl.py` | pyautogui 键盘模拟 |
| `wb_bridge/workbuddy_if.py` | WorkBuddy 集成（文本注入/响应检测） |
| `wb_bridge/tts_engine.py` | TTS 引擎封装 |
| `wb_bridge/wb_protocol.py` | PC 端协议解析/组包 |
| `wb_bridge/config.py` | 配置（Whisper 模型、BLE 设备名等）|

---

## 7. 已知限制

| 项目 | 说明 |
|------|------|
| Wi-Fi 禁用 | 当前固件 Wi-Fi 被禁用，仅 BLE 通信 |
| Whisper 模型 | 默认用 `base` 模型（需 ~1GB RAM）；GPU 可用 `medium` |
| TTS | 暂用 PC 端 TTS + BLE 传输 PCM，延迟较高 |
| WorkBuddy 弹窗检测 | 依赖轮询剪贴板，精度有限 |
| 中文 ASR | Whisper base 中文能力尚可，复杂术语可能出错 |

---

## 8. 验收标准

- [ ] PC Bridge 能连接 ESP32 BLE 并收发消息
- [ ] 长按 OK 录音，松开后文字注入 WorkBuddy 输入框
- [ ] WorkBuddy 响应后，硬件屏幕显示通知
- [ ] UP/DOWN/OK 按钮能控制 WorkBuddy 弹窗选项
- [ ] 固件 UI 符合重新设计规范（非 demo 菜单）
