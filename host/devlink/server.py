"""单实例服务：Terminal + 本机 TCP 控制口。

为什么需要它
------------
串口是独占资源。WorkBuddy 的 hooks 是**每个事件起一个进程**，MCP 服务是**每会话一个进程**，
如果各自去 open("COM4")，必然互相踢掉。所以只让本服务持有串口，
其它一切（hooks / MCP / 命令行）都通过 127.0.0.1:37821 说一句话。

单实例靠"端口占用"实现：绑不上说明已经有一个在跑。
"""
from __future__ import annotations

import asyncio
import json
import os
import signal
import sys
import time

from . import codec, proto
from .session import TAGS_STATE
from .terminal import Terminal, log

HOST = "127.0.0.1"
PORT = int(os.getenv("WB_DAEMON_PORT", "37821"))


class AlreadyRunning(Exception):
    pass


def _client(req: dict, timeout: float = 30.0) -> dict:
    """同步发一条控制指令（给 hook / cli 用，它们不需要 asyncio）。"""
    import socket

    try:
        with socket.create_connection((HOST, PORT), timeout=2.0) as s:
            s.sendall((json.dumps(req, ensure_ascii=False) + "\n").encode("utf-8"))
            if not req.get("_wait", True):
                return {"ok": True}
            s.settimeout(timeout)
            buf = b""
            while b"\n" not in buf:
                chunk = s.recv(4096)
                if not chunk:
                    break
                buf += chunk
            return json.loads(buf.split(b"\n")[0].decode("utf-8")) if buf else {"ok": False}
    except OSError:
        return {"ok": False, "error": "daemon 未运行"}


def running(timeout: float = 0.6) -> bool:
    import socket
    try:
        with socket.create_connection((HOST, PORT), timeout=timeout):
            return True
    except OSError:
        return False


class Server:
    def __init__(self):
        self.term = Terminal()
        self._server: asyncio.AbstractServer | None = None

    async def dispatch(self, req: dict) -> dict:
        op = req.get("op", "")
        t = self.term
        p = req

        if op == "sessions":
            return {"ok": True, "sessions": TAGS_STATE.describe()}

        if op == "forget":
            TAGS_STATE.reset()
            await t.header("WorkBuddy")
            return {"ok": True}

        if op == "ping":
            return {"ok": True, "serial": t.link.connected, "port": t.link.port,
                    "fw": t.fw, "state": t.state}

        if op == "status":
            sid = str(p.get("session_id") or "")
            if TAGS_STATE.note(sid):
                # 刚从单会话变多会话：历史行的归属已经无从判断，
                # 与其显示带错误归属的内容，不如清屏重来（不假装成功）。
                await t.clear()
                await t.reply(TAGS_STATE.tag_text(sid, "新会话加入，屏幕已分区"))
            await t.header(TAGS_STATE.header(sid))
            await t.status(p.get("state", "idle"),
                           TAGS_STATE.tag_text(sid, p.get("text", "")))
            return {"ok": True}

        if op == "reply":
            sid = str(p.get("session_id") or "")
            TAGS_STATE.note(sid)
            await t.reply(TAGS_STATE.tag_text(sid, p.get("text", "")))
            return {"ok": True}

        if op == "notify":
            sid = str(p.get("session_id") or "")
            TAGS_STATE.note(sid)
            await t.notify(TAGS_STATE.tag_text(sid, p.get("title", "通知")),
                           p.get("body", ""), p.get("priority", "normal"))
            return {"ok": True}

        if op == "clear":
            await t.clear()
            return {"ok": True}

        if op == "speak":
            return {"ok": True, "result": await t.speak(p.get("text", ""))}

        if op == "ask":
            return {"ok": True, "answer": await t.ask(p.get("text", "确认？"),
                                                      float(p.get("timeout", 60)))}

        if op == "approve":
            r = await t.approve(p.get("tool", ""), p.get("detail", ""),
                                float(p.get("timeout", 100)))
            return {"ok": True, **r}

        if op == "type":
            return {"ok": True,
                    "result": await t.type_text(p.get("text", ""), p.get("submit", True))}

        if op == "listen":
            return {"ok": True, "text": await t.listen(float(p.get("timeout", 60)))}

        if op == "talk":
            # 走真实按键路径：开/关麦克风，停止时触发「识别 → 注入 WorkBuddy」
            t.link.send(proto.HOST_DEBUG_TALK, on=bool(p.get("on", True)))
            return {"ok": True}

        if op == "gain":
            await t.set_mic_gain(float(p.get("db", 15)))
            return {"ok": True}

        if op == "speak_last":
            # 每轮结束时把结论念出来。hook 的 Stop 事件带 transcript_path，
            # 服务侧解析出"最后一段值得念的话"，再推流到设备扬声器。
            #
            # 三个关键设计：
            # 1) 不 await：一次播报 15~25 秒，而 hook 的 timeout 只有 8 秒。
            #    这里排到后台任务里，立刻回包，hook 不会被拖死。
            # 2) 用开关控制：WB_SPEAK_REPLY=0 可关（默认开）。
            # 3) 这段文字只念一次，不进对话记录——它是"听觉提示"，不是内容。
            if os.getenv("WB_SPEAK_REPLY", "1").strip().lower() in (
                    "0", "false", "off", "no"):
                return {"ok": True, "skipped": "WB_SPEAK_REPLY=0"}
            from . import digest

            path = p.get("transcript_path", "")
            max_chars = int(p.get("max_chars", 120))
            text = await asyncio.to_thread(
                digest.last_reply_for_speech, path, max_chars)
            if not text:
                return {"ok": True, "spoken": "", "reason": "transcript 里没有可念的内容"}
            asyncio.create_task(t.speak(text))
            log(f"播报本轮结论：{text[:40]}…")
            return {"ok": True, "spoken": text}

        if op == "record":
            sec = float(p.get("sec", 5))
            pcm = await t.record(sec)
            # 诊断用同一套预处理：先切掉 codec 上电爆音，
            # 否则"削顶 / rms"这些指标反映的是爆音而不是人声，看了会误判。
            pcm_trim = codec.trim_transient(pcm, 16000) if pcm else b""
            info = {}
            clean = b""
            if pcm:
                import numpy as np
                from . import codec as _c
                from . import speech
                raw = np.frombuffer(pcm_trim or pcm, dtype=np.int16).astype(np.float32)
                ev = raw[0::2] - raw[0::2].mean()
                od = raw[1::2] - raw[1::2].mean()
                c = _c.prepare_for_asr(pcm_trim or pcm, 16000)
                if c.size:
                    clean = np.clip(c * 32767, -32768, 32767).astype(np.int16).tobytes()
                info = {
                    "dc": round(float(raw.mean())),
                    "raw_rms": round(float(np.sqrt(np.mean(raw ** 2)))),
                    "peak": int(np.abs(raw).max()),
                    "ac_rms": round(float(np.sqrt(np.mean((raw - raw.mean()) ** 2)))),
                    "left_rms": round(float(np.sqrt(np.mean(ev ** 2)))),
                    "right_rms": round(float(np.sqrt(np.mean(od ** 2)))),
                    "clean_rms": round(float(np.sqrt(np.mean(c ** 2))) * 32768) if c.size else 0,
                    "voiced": bool(c.size),
                }
                # 存原始+增强两份，方便回放判断"没录到"还是"识别不准"
                # 注意：这里**不要** import os —— 模块顶层已导入。
                # 在函数体内写 import os 会让 os 在整个 dispatch 里变成局部变量，
                # 于是前面分支（如 speak_last 读环境变量）会撞 UnboundLocalError。
                import time as _t
                import wave
                d = os.path.join(os.path.dirname(os.path.dirname(
                    os.path.abspath(__file__))), "recordings")
                os.makedirs(d, exist_ok=True)
                info["raw_wav"] = os.path.join(d, "last_raw.wav")
                with wave.open(info["raw_wav"], "wb") as w:
                    w.setnchannels(1)
                    w.setsampwidth(2)
                    w.setframerate(16000)
                    w.writeframes(pcm)
                if clean:
                    info["clean_wav"] = os.path.join(d, "last_clean.wav")
                    with wave.open(info["clean_wav"], "wb") as w:
                        w.setnchannels(1)
                        w.setsampwidth(2)
                        w.setframerate(16000)
                        w.writeframes(clean)
                info["saved_at"] = _t.strftime("%H:%M:%S")
            text = ""
            if pcm and p.get("asr", True):
                from . import speech
                text = await speech.transcribe_async(pcm)
            return {"ok": True, "samples": len(pcm) // 2,
                    "sec": round(len(pcm) / 2 / 16000, 2), "text": text, "stats": info}

        if op == "last":
            return {"ok": True, "text": t.last_text}

        return {"ok": False, "error": f"未知 op: {op}"}

    async def _handle(self, reader: asyncio.StreamReader, writer: asyncio.StreamWriter):
        try:
            while True:
                line = await reader.readline()
                if not line:
                    return
                line = line.strip()
                if not line:
                    continue
                try:
                    req = json.loads(line.decode("utf-8"))
                except Exception as e:
                    resp = {"ok": False, "error": f"bad json: {e}"}
                else:
                    # 每条指令都留痕：hook 有没有真的连上来、发了什么，全靠这行日志判断。
                    # 只记一行摘要，避免日志被长文本撑爆。
                    op = req.get("op", "?")
                    detail = ""
                    for k in ("text", "state", "tool", "title", "db"):
                        v = req.get(k)
                        if isinstance(v, str) and v:
                            detail = " " + v.replace("\n", " ")[:60]
                            break
                    log(f"← {op}{detail}")
                    try:
                        resp = await self.dispatch(req)
                    except Exception as e:
                        log(f"op={req.get('op')} 失败：{e}")
                        resp = {"ok": False, "error": f"{type(e).__name__}: {e}"}
                writer.write((json.dumps(resp, ensure_ascii=False) + "\n").encode("utf-8"))
                await writer.drain()
        except (ConnectionResetError, BrokenPipeError):
            pass
        finally:
            try:
                writer.close()
            except Exception:
                pass

    async def run(self) -> None:
        try:
            self._server = await asyncio.start_server(self._handle, HOST, PORT)
        except OSError as e:
            raise AlreadyRunning(str(e))

        ok = await self.term.start()
        # 固件现在会把增益记在 s_in_gain 里、每次 open codec 后重新施加，
        # 所以这里设一次就一直有效（以前设了会被下次 open 复位成 15dB）。
        if ok and os.getenv("WB_MIC_GAIN"):
            try:
                db = float(os.environ["WB_MIC_GAIN"])
                await asyncio.sleep(0.5)
                await self.term.set_mic_gain(db)
                log(f"麦克风增益 {db:g} dB")
            except ValueError:
                pass
        log(f"控制口 {HOST}:{PORT}；串口 {self.term.link.port or '(未接)'} "
            f"{'已连' if ok else '未连（插上设备自动重试）'}"
            f"{'  fw=' + self.term.fw if self.term.fw else ''}")

        loop = asyncio.get_running_loop()
        for sig in (signal.SIGINT, signal.SIGTERM):
            try:
                loop.add_signal_handler(sig, lambda: asyncio.create_task(self.shutdown()))
            except NotImplementedError:
                pass

        pump = asyncio.create_task(self.term.run())
        try:
            await pump
        finally:
            await self.term.stop()

    async def shutdown(self) -> None:
        self.term._running = False


def load_env(path: str = "") -> None:
    """读同目录的 .env（没有 python-dotenv 也能用，不覆盖已有环境变量）。"""
    path = path or os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".env")
    if not os.path.exists(path):
        return
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#") or "=" not in line:
                    continue
                k, v = line.split("=", 1)
                k, v = k.strip(), v.strip().strip('"').strip("'")
                if k and k not in os.environ:
                    os.environ[k] = v
    except OSError:
        pass


def main() -> int:
    load_env()
    if os.name == "nt":
        # 让 Ctrl+C 直接退出，不要弹"是否终止批处理"
        pass
    srv = Server()
    try:
        asyncio.run(srv.run())
    except AlreadyRunning:
        print(f"已有实例在 {HOST}:{PORT} 运行，本次不启动。", file=sys.stderr)
        return 0
    except KeyboardInterrupt:
        log("已停止")
    return 0


if __name__ == "__main__":
    sys.exit(main())
