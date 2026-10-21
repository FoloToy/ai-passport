"""MCP stdio 服务：把工具调用转发给常驻服务。

它自己**不碰串口**——所有动作都通过 127.0.0.1:37821 转给 devlink.server。
这样 MCP 和 hooks 可以同时活着，不会互相抢 COM4。
"""
from __future__ import annotations

import asyncio
import os
import sys

from .server import _client
from .terminal import log


def _call(req: dict, timeout: float = 30.0) -> dict:
    return _client(req, timeout)


def build():
    from contextlib import asynccontextmanager
    from mcp.server.fastmcp import FastMCP

    @asynccontextmanager
    async def lifespan(server):
        info = _call({"op": "ping"}, 3)
        if info.get("ok"):
            log(f"[MCP] 已接入服务 fw={info.get('fw')} port={info.get('port')}")
        else:
            log("[MCP] 服务未运行，先执行：python -m devlink.server")
        yield {}

    mcp = FastMCP("ai-passport", lifespan=lifespan)

    @mcp.tool()
    async def wb_ping() -> str:
        """探测设备链路。返回固件版本，未连通返回 no_device。"""
        r = await asyncio.to_thread(_call, {"op": "ping"}, 4)
        return r.get("fw") or ("no_device" if not r.get("ok") else "connected")

    @mcp.tool()
    async def wb_status(state: str, text: str = "") -> str:
        """推送状态药丸。state: idle | thinking | waiting | alert。"""
        await asyncio.to_thread(_call, {"op": "status", "state": state, "text": text}, 5)
        return "ok"

    @mcp.tool()
    async def wb_reply(text: str, speak: bool = False) -> str:
        """在设备对话记录里加一行。speak=True 同时用设备扬声器读出来。"""
        await asyncio.to_thread(_call, {"op": "reply", "text": text}, 5)
        if speak:
            await asyncio.to_thread(_call, {"op": "speak", "text": text}, 180)
        return "ok"

    @mcp.tool()
    async def wb_speak(text: str) -> str:
        """只用设备扬声器播报（屏幕上不显示）。"""
        r = await asyncio.to_thread(_call, {"op": "speak", "text": text}, 180)
        return r.get("result", "failed")

    @mcp.tool()
    async def wb_notify(title: str, body: str = "", priority: str = "normal") -> str:
        """推通知卡片 + 提示音。priority: normal | high | urgent（后者需按 OK 关闭）。"""
        await asyncio.to_thread(_call, {"op": "notify", "title": title,
                                        "body": body, "priority": priority}, 5)
        return "ok"

    @mcp.tool()
    async def wb_ask(text: str, timeout_sec: int = 90) -> str:
        """在设备上弹确认框，等用户按 OK(确认)/↓+OK(拒绝)。

        返回 confirm | cancel | timeout | error:*。适合"要不要继续/选哪个"这类决策。
        """
        r = await asyncio.to_thread(_call, {"op": "ask", "text": text,
                                            "timeout": timeout_sec}, timeout_sec + 15)
        return r.get("answer", "error")

    @mcp.tool()
    async def wb_listen(timeout_sec: int = 90) -> str:
        """等用户在设备上按住 OK 说一句，返回识别出的文字（不会自动发给 WorkBuddy）。"""
        r = await asyncio.to_thread(_call, {"op": "listen", "timeout": timeout_sec},
                                    timeout_sec + 15)
        return r.get("text", "timeout")

    @mcp.tool()
    async def wb_type(text: str, submit: bool = True) -> str:
        """把文字直接注入 WorkBuddy 输入框（剪贴板 + Ctrl+V），submit=True 顺带回车。"""
        r = await asyncio.to_thread(_call, {"op": "type", "text": text, "submit": submit}, 20)
        return r.get("result", "failed")

    @mcp.tool()
    async def wb_last_heard() -> str:
        """最近一次设备语音识别到的文字。"""
        r = await asyncio.to_thread(_call, {"op": "last"}, 5)
        return r.get("text", "")

    @mcp.tool()
    async def wb_clear() -> str:
        """清屏并清空对话记录。"""
        await asyncio.to_thread(_call, {"op": "clear"}, 5)
        return "ok"

    return mcp


def main() -> None:
    build().run(transport="stdio")


if __name__ == "__main__":
    main()
