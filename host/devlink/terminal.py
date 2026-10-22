"""终端语义层：把设备的原始事件翻译成"人想做的事"。

设备是哑终端——它只会：
  · 显示 PC 让它显示的东西（状态 / 日志行 / 通知卡 / 确认框）
  · 上报按键、上报麦克风音频、在确认框里给出 confirm/cancel

本模块负责所有判断：录音该不该送去识别、识别完要不要插进 WorkBuddy、
审批该不该放行、播报该按什么节奏喂给设备。
"""
from __future__ import annotations

import asyncio
import base64
import os
import sys
import time
import uuid

from . import codec, inject, proto, speech
from .link import Link

# 设备播报环形缓冲 32KB = 2s @8k/16bit；发送领先 1.2s 就不会溢出
TTS_RING_SEC = 2.0
TTS_LEAD_SEC = 1.2
TTS_CHUNK_ADPCM = 512            # 128ms 一块

# 只读工具不打扰用户（审批模式下自动放行，但仍在屏幕上留痕）
READ_ONLY = {"Read", "Glob", "Grep", "TodoWrite", "Task", "WebSearch", "WebFetch",
             "NotebookRead", "LS"}


LOG_PATH = os.getenv("WB_LOG", os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "events.log"))
LOG_MAX = 2 * 1024 * 1024


def log(*a):
    """写 stderr，同时**追加**到 events.log。

    为什么要自己写文件：以前靠 shell 的 `> srv.log`，每次重启就把历史清空。
    结果"用户到底按没按 OK"这种问题，重启一次证据就没了——今天正是因此反复误判。
    """
    line = time.strftime("%H:%M:%S") + " " + " ".join(str(x) for x in a)
    print(line, file=sys.stderr, flush=True)
    try:
        if os.path.exists(LOG_PATH) and os.path.getsize(LOG_PATH) > LOG_MAX:
            os.replace(LOG_PATH, LOG_PATH + ".1")
        with open(LOG_PATH, "a", encoding="utf-8") as f:
            f.write(time.strftime("%Y-%m-%d ") + line + "\n")
    except Exception:
        pass


class Terminal:
    def __init__(self, port: str = "", *, autopaste: bool = True):
        self.link = Link(port)
        self.autopaste = autopaste
        self.fw = ""

        # 上行音频累积
        self._audio = bytearray()
        self._audio_blocks = 0
        self._recording = False
        self._capture_fut: asyncio.Future | None = None

        # 审批 / 询问的等待者
        self._asks: dict[str, asyncio.Future] = {}
        self._ask_lock = asyncio.Lock()

        # 播报串行化：两次播报并发会把设备环形缓冲写乱
        self._tts_lock = asyncio.Lock()

        # 说话结果队列（给 wb_listen 用）
        self.heard: asyncio.Queue[str] = asyncio.Queue()

        # 状态
        self.state = "idle"
        self.last_text = ""
        # 麦克风增益（dB）。录完自检会在此基础上自动加减。
        self.gain = float(os.getenv("WB_MIC_GAIN", "30") or 30)
        self._pending: asyncio.Task | None = None
        self._hb: asyncio.Task | None = None
        self._running = False

    # ------------------------------------------------ 生命周期

    async def start(self) -> bool:
        ok = self.link.open()
        self._running = True
        self._hb = asyncio.create_task(self._heartbeat())
        if ok:
            self.fw = await self.ping() or ""
            await self.clear()
            await self.status("idle", "伴侣已就绪")
        return ok

    async def stop(self) -> None:
        self._running = False
        if self._hb:
            self._hb.cancel()
            self._hb = None
        self.link.close()

    async def _heartbeat(self) -> None:
        """固件 30s 收不到消息就判 PC 离线，所以 10s 一次心跳。"""
        while self._running:
            await asyncio.sleep(proto.HEARTBEAT_SEC)
            if not self.link.connected:
                if self.link.open():
                    log(f"串口已重连：{self.link.port}")
                    self.fw = await self.ping() or ""
            else:
                self.link.send(proto.HOST_HEARTBEAT)

    # ------------------------------------------------ 事件循环

    async def run(self) -> None:
        """持续消费设备上行。断线时自己重连。"""
        while self._running:
            if not self.link.connected:
                await asyncio.sleep(2.0)
                if self.link.open():
                    log(f"串口已重连：{self.link.port}")
                    self.fw = await self.ping() or ""
                    await self.status("idle", "伴侣已重连")
                continue
            msg = await self.link.next_msg(0.2)
            if msg is None:
                continue
            try:
                await self._on_msg(msg)
            except Exception as e:
                log(f"处理 {msg.get('type')} 出错：{e}")

    async def _on_msg(self, msg: dict) -> None:
        kind = msg["type"]
        p = msg.get("payload") or {}

        if kind == proto.DEV_AUDIO:
            raw = p.get("b")
            if not isinstance(raw, str):
                return
            try:
                block = base64.b64decode(raw)
            except Exception:
                return
            # 帧格式：[seq(1)][len(2 LE)][adpcm block]
            if len(block) < 4:
                return
            n = int.from_bytes(block[1:3], "little")
            body = block[3:3 + n] if 0 < n <= len(block) - 3 else block[3:]
            pcm = codec.AdpcmDecoder().decode(body)
            if pcm:
                if not self._recording:
                    self._recording = True
                    log("音频开始上行")
                    await self.status("waiting", "聆听中…")
                self._audio += pcm
                self._audio_blocks += 1

        elif kind == proto.DEV_AUDIO_END:
            self._recording = False
            # 上一轮的收尾 audio_end 可能还在路上；没收到任何音频块时不要误判为本轮结束
            if self._capture_fut is not None and self._audio_blocks == 0:
                return
            n = len(self._audio) // 2
            blocks = self._audio_blocks
            pcm = bytes(self._audio)
            self._audio = bytearray()
            self._audio_blocks = 0
            log(f"收到语音：{n} 采样（{n / 16000:.1f}s，{blocks} 块）")
            if self._capture_fut is not None:        # 远程录音：只取样，不识别不注入
                fut, self._capture_fut = self._capture_fut, None
                if not fut.done():
                    fut.set_result(pcm)
            elif n >= 4000:                          # 至少 0.25s 才值得识别
                asyncio.create_task(self._handle_voice(pcm))
            else:
                await self.status("idle", "太短了，没听清")

        elif kind == proto.DEV_ANSWER:
            fut = self._asks.pop(p.get("id", ""), None)
            if fut and not fut.done():
                fut.set_result(p.get("v", "cancel"))

        elif kind == proto.DEV_PONG:
            self.fw = p.get("fw", "")

        elif kind == proto.DEV_KEY:
            btn, ev = p.get("button", ""), p.get("event", "")
            # 按键必须留痕：否则"按了没反应"根本分不清是按键没上报、
            # 还是上报了但没采到音——这两种的修法完全不同。
            log(f"按键 {btn}/{ev}")
            if btn == "ok" and ev == "press" and not self._asks:
                await self.status("waiting", "聆听中…")

    # 增益自适应：实测同一档 30dB 在不同环境下既出现过 peak=6161（安全），
    # 也出现过 peak=32768（削顶）。削顶的波形被削平，识别必然很差，
    # 而且从识别结果完全看不出是增益问题。所以录完立刻自检电平并自我修正。
    GAIN_MIN, GAIN_MAX = 6.0, 30.0

    # 静音阈值：这块板子 30dB 下的底噪实测 rms ≈ 180~230，
    # 真实近距离说话 rms ≈ 2500。阈值必须高于底噪，否则底噪会被喂进识别器，
    # Paraformer 在近乎空的输入上会吐幻觉——实测 rms=169 时它照样输出
    # "测试测试测试…"这种完整句子，看起来像识别成功，其实是编的。
    SILENT_RMS = 400

    # 削顶判定：分两档。
    #   ≥ 3‰：轻微削顶——照样送识别（实测 2‰ 时 10 个字全对），只是顺手降增益。
    #   ≥ 25‰：严重削顶——波形被削平，识别不可信，直接拒绝并请重说。
    # 早先把 3‰ 就当严重削顶，结果 rms=3490 这种其实能认的音频也被拒了。
    CLIP_HINT_PERMILLE = 3
    CLIP_BAD_PERMILLE = 25

    def _audio_health(self, pcm: bytes) -> tuple[str, int, int]:
        """返回 (状态, 削顶千分比, ac_rms)。

        状态：bad_clip / clip / silent / ok
        """
        import numpy as np

        x = np.frombuffer(pcm, dtype=np.int16)
        if x.size == 0:
            return "silent", 0, 0
        clip = int((np.abs(x) >= 32700).sum())
        permille = clip * 1000 // x.size
        f = x.astype(np.float32)
        rms = float(np.sqrt(np.mean((f - f.mean()) ** 2)))
        if permille >= self.CLIP_BAD_PERMILLE:
            return "bad_clip", permille, int(rms)
        if permille >= self.CLIP_HINT_PERMILLE:
            return "clip", permille, int(rms)
        if rms < self.SILENT_RMS:
            return "silent", permille, int(rms)
        return "ok", permille, int(rms)

    async def adjust_gain(self, delta: float) -> float:
        """运行时调增益并记住，返回新值。"""
        self.gain = max(self.GAIN_MIN, min(self.GAIN_MAX, self.gain + delta))
        self.link.send(proto.HOST_MIC_GAIN, db=self.gain)
        log(f"增益调整为 {self.gain:g} dB")
        return self.gain

    async def _handle_voice(self, pcm: bytes) -> None:
        # 先切掉 codec 上电爆音：这块板子每次开录的头 100ms 会有一串满幅样本，
        # 既会把"削顶"判据打满，又会被当成语音开头送去识别（结果开头多几个乱字）。
        raw_len = len(pcm) // 2
        pcm = codec.trim_transient(pcm, 16000)
        trimmed_ms = (raw_len - len(pcm) // 2) * 1000 // 16000
        if trimmed_ms > 0:
            log(f"已切掉开头瞬态 {trimmed_ms}ms")

        state, permille, rms = self._audio_health(pcm)
        log(f"电平自检：{state} 削顶={permille}‰ rms={rms} 增益={self.gain:g}dB")

        # 把每次真实语音存盘。没有这一步，"识别不准"永远只能靠猜：
        # 存下来才能对同一段音频换模型/换参数做 A/B，也才能回放确认到底说了什么。
        try:
            import wave
            d = os.path.join(os.path.dirname(os.path.dirname(
                os.path.abspath(__file__))), "recordings")
            os.makedirs(d, exist_ok=True)
            path = os.path.join(d, "last_voice.wav")
            with wave.open(path, "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(16000)
                w.writeframes(pcm)
            log(f"语音已存盘 {path}（{len(pcm) // 2} 采样）")
        except Exception as e:
            log(f"存盘失败：{e}")

        if state == "bad_clip":
            # 严重削顶：波形被削平，识别不可信，别把垃圾注入进去
            await self.adjust_gain(-3.0)
            await self.status("alert", f"声音太大，已降到 {self.gain:g}dB，请重说")
            await self.reply("声音太大削平了，已降低增益，请再说一次")
            return
        if state == "silent":
            await self.status("alert", "没听到声音")
            await self.reply("没听到声音，靠近一点再说")
            return
        if state == "clip":
            # 轻微削顶仍可识别（实测 2‰ 时逐字全对），只顺手降一档，别打断用户
            await self.adjust_gain(-3.0)

        await self.status("thinking", "识别中…")
        text = await speech.transcribe_async(pcm)
        if not text:
            await self.status("idle", "没听清，再说一次")
            await self.reply("没听清，再说一次")
            return

        self.last_text = text
        await self.reply(f"你：{text}")
        await self.heard.put(text)

        if not self.autopaste:
            await self.status("idle", "已识别（未自动发送）")
            return

        result = await asyncio.to_thread(inject.send_text, text, submit=True)
        # 注入结果必须留痕：以前只记"收到的指令"，不记下发是否成功，
        # 于是"文字到底发没发出去"只能靠用户在屏幕上猜。
        log(f"注入结果：{result}（{len(text)} 字）")
        if result == "ok":
            await self.status("thinking", "已发给 WorkBuddy")
        elif result == "wrote_no_submit":
            # 文字已进输入框，只是发送位被"停止"占着（WorkBuddy 正在回话）。
            # 交给后台盯着，一旦空闲就点发送——不用人守着。
            await self.status("waiting", "已填入，等回话结束自动发送")
            await self.reply("文字已填入输入框，等这轮结束会自动发出")
            self._pending = asyncio.create_task(self._flush_pending(text))
        elif result == "no_window":
            await self.status("alert", "先打开 WorkBuddy")
        elif result == "no_input":
            await self.status("alert", "找不到输入框")
        else:
            await self.status("alert", f"注入失败：{result}")

    async def _flush_pending(self, text: str, limit: float = 120.0) -> None:
        """文字已在输入框里，只负责等空闲后点发送。

        不要在这里重新输入：那会重复写一遍。输入只在 _handle_voice 里做一次。

        超时后必须清掉，不能留在框里：
        留在框里的话，用户之后随手按一下回车就会把这条"过期的话"发出去——
        实测就发生过（测试残留被当成新消息发进了对话）。
        丢一句待发内容比发出幽灵消息好得多，所以超时即清空并明确提示重说。
        """
        t0 = time.time()
        while self._running and time.time() - t0 < limit:
            await asyncio.sleep(2.0)
            # 必须区分"读到了空内容"和"读不到"：
            # WorkBuddy 被其它窗口遮挡时无障碍树被销毁，读不到是常态，不能当成已发出。
            ok, cur = await asyncio.to_thread(inject.probe_value)
            if not ok:
                continue
            # 判空必须排除 placeholder：空输入框时 Chromium 会把提示语当值返回，
            # 直接比空串会让"已发出"永远判不出来。
            if inject.is_blank_value(cur) or text.strip()[:12] not in cur:
                log("待发送消息已发出")
                await self.status("thinking", "已发给 WorkBuddy")
                return
            if await asyncio.to_thread(inject.is_busy):
                continue                      # 还在回话，发送键不存在
            try:
                if await asyncio.to_thread(inject.press_send):
                    await asyncio.sleep(0.6)
                    ok2, cur2 = await asyncio.to_thread(inject.probe_value)
                    if ok2 and (inject.is_blank_value(cur2)
                                or text.strip()[:12] not in cur2):
                        log("待发送消息已补发")
                        await self.status("thinking", "已发给 WorkBuddy")
                        return
            except Exception as e:
                log(f"补发失败：{e}")

        # 超时：把内容中和成单个空格，别让它变成下一条幽灵消息。
        # 必须用 clear_soft（走 WM_CHAR），不能用 SetValue：
        # SetValue 只清 DOM，React state 仍留着整句话，之后一次重渲染就会把原句
        # 写回输入框，用户按回车就又发一遍——实测连发了好几次。
        # clear_soft 会把 React state 同步成 " "，而纯空白发送不出去，残留因此无害。
        log(f"发送超时，中和未发送内容：{text[:20]!r}")
        try:
            await asyncio.to_thread(inject.clear_soft)
        except Exception as e:
            log(f"清理失败：{e}")
        await self.status("alert", "没发出去，已清空，请重说一次")
        await self.reply("这句没发出去，已清空")

    # ------------------------------------------------ 动作

    async def ping(self, timeout: float = 3.0) -> str:
        """探测链路，成功返回固件版本号。"""
        while not self.link.drain():                       # 清掉旧消息
            break
        self.link.send(proto.HOST_PING)
        t0 = time.time()
        while time.time() - t0 < timeout:
            msg = await self.link.next_msg(0.1)
            if msg and msg["type"] == proto.DEV_PONG:
                return (msg.get("payload") or {}).get("fw", "ok")
        return ""

    async def status(self, state: str, text: str = "") -> None:
        self.state = state
        self.link.send(proto.HOST_STATUS, state=state, text=text)

    async def header(self, text: str) -> None:
        """标题栏文本（多会话时显示会话标签与数量）。

        老固件不认识这条消息，`dispatch_message` 的 default 分支只打一行
        LOGD 就丢掉，所以先发无副作用——不需要固件与主机同版本。
        """
        t = (text or "").strip()
        if t:
            self.link.send(proto.HOST_HEADER, text=t[:24])

    async def reply(self, text: str) -> None:
        self.link.send(proto.HOST_REPLY, text=text)

    async def notify(self, title: str, body: str = "", priority: str = "normal") -> None:
        self.link.send(proto.HOST_NOTIFY, title=title, body=body, priority=priority)

    async def clear(self) -> None:
        self.link.send(proto.HOST_CLEAR)

    async def ask(self, text: str, timeout: float = 60.0) -> str:
        """弹确认框，返回 confirm / cancel / timeout。"""
        if not self.link.connected:
            return "error:not_connected"
        aid = uuid.uuid4().hex[:12]
        fut = asyncio.get_running_loop().create_future()
        self._asks[aid] = fut
        if not self.link.send(proto.HOST_ASK, id=aid, text=text):
            self._asks.pop(aid, None)
            return "error:send_failed"
        try:
            async with self._ask_lock:
                return await asyncio.wait_for(fut, timeout)
        except asyncio.TimeoutError:
            self._asks.pop(aid, None)
            await self.clear()
            return "timeout"

    async def approve(self, tool: str, detail: str = "", timeout: float = 100.0) -> dict:
        """工具审批：把这次调用推到屏幕上等用户按键。

        决策取舍（重要）：
          · 只读工具 → allow（不打扰，但屏上留一行）
          · 用户按 OK  → allow
          · 用户按 ↓+OK → deny
          · 超时/设备不在 → **ask**（交回 WorkBuddy 自己的权限弹窗）
            绝不因为"没人在"就擅自放行或擅自拒绝。
        """
        if tool in READ_ONLY:
            await self.reply(f"· {tool} {detail[:40]}")
            return {"decision": "allow"}

        ans = await self.ask(f"允许 {tool}？\n{detail[:120]}", timeout)
        if ans == "confirm":
            return {"decision": "allow"}
        if ans == "cancel":
            return {"decision": "deny", "reason": "用户在 AI PASSPORT 上按了拒绝"}
        return {"decision": "ask"}

    async def speak(self, text: str) -> str:
        """把文字合成语音推给设备扬声器。

        加锁：播报是"边合成边推流"，一次 15~25 秒。
        两轮回复挨得近时会出现两个播报并发推流，设备环形缓冲被交错写，
        出来的声音是乱的。串行化最省事。
        """
        text = (text or "").strip()
        if not text:
            return "empty"
        async with self._tts_lock:
            # 新一轮开始前先让设备停掉上一段，避免叠音
            self.link.send(proto.HOST_TTS_STOP)
            pcm = await asyncio.to_thread(speech.tts_pcm, text, speech.VOICE_HZ)
            if not pcm:
                return "tts_failed"
            return await self._stream_pcm(pcm)

    async def _stream_pcm(self, pcm: bytes) -> str:
        import numpy as np

        hz = speech.VOICE_HZ
        samples = len(pcm) // 2
        enc = codec.AdpcmEncoder()
        per_block = TTS_CHUNK_ADPCM * 2          # 每块覆盖的样本数

        if not self.link.send(proto.HOST_TTS_BEGIN, hz=hz, a=1):
            return "not_connected"

        t0 = time.time()
        sent = 0
        seq = 0
        for off in range(0, samples, per_block):
            arr = np.frombuffer(pcm[off * 2:(off + per_block) * 2], dtype=np.int16)
            if arr.size == 0:
                break
            b64 = base64.b64encode(enc.encode_block(arr)).decode("ascii")
            if not self.link.send(proto.HOST_TTS_CHUNK, s=seq, b=b64):
                return "send_failed"
            seq += 1
            sent += arr.size
            # 领先播放 TTS_LEAD_SEC 发包，设备环形缓冲就不会溢
            ahead = sent / hz - (time.time() - t0)
            if ahead > TTS_LEAD_SEC:
                await asyncio.sleep(ahead - TTS_LEAD_SEC)

        self.link.send(proto.HOST_TTS_END)
        log(f"播报已推送 {samples} 采样 / {samples / hz:.1f}s（{seq} 块）")
        return "ok"

    async def listen(self, timeout: float = 60.0) -> str:
        """等用户按住设备说一句，返回识别文字。"""
        try:
            return await asyncio.wait_for(self.heard.get(), timeout)
        except asyncio.TimeoutError:
            return "timeout"

    async def set_mic_gain(self, db: float) -> None:
        """运行时改麦克风输入增益。削顶时往下调（默认 30 dB）。"""
        self.gain = max(6.0, min(30.0, float(db)))
        self.link.send(proto.HOST_MIC_GAIN, db=self.gain)

    async def type_text(self, text: str, submit: bool = True) -> str:
        return await asyncio.to_thread(inject.send_text, text, submit=submit)

    async def record(self, seconds: float = 5.0) -> bytes:
        """远程开麦录一段，返回 PCM（不识别、不注入）。给自检用。"""
        if not self.link.connected:
            return b""
        self._audio = bytearray()
        self._audio_blocks = 0
        fut = asyncio.get_running_loop().create_future()
        self._capture_fut = fut
        self.link.send(proto.HOST_DEBUG_TALK, on=True)
        await self.status("waiting", "远程录音中…")
        await asyncio.sleep(max(1.0, seconds))
        self.link.send(proto.HOST_DEBUG_TALK, on=False)
        try:
            pcm = await asyncio.wait_for(fut, 12.0)
        except asyncio.TimeoutError:
            pcm = bytes(self._audio)
        finally:
            self._capture_fut = None
        await self.status("idle", "录音结束")
        return pcm
