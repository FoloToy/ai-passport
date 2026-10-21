"""从 WorkBuddy 的 transcript（jsonl）里取出"最后一段值得念出来的话"。

为什么需要这个模块
------------------
Stop hook 的 payload 里有 `transcript_path`，指向会话的 jsonl 对话记录。
有了它，设备就能在每轮结束时把结论**念出来**——不用看屏幕也知道结果。
在这之前，播报能力（`terminal.speak`）写好了却从没被用过，
因为没有任何地方知道"AI 刚说了什么"。

transcript 的行格式（实测）
--------------------------
    {"type":"message","role":"assistant","content":[{"type":"output_text","text":"..."}]}
    {"type":"reasoning", ...}      ← 思考过程，不念
    {"type":"function_call", ...}  ← 工具调用，不念
    {"type":"message","role":"user", ...}

所以取数规则：从尾部倒着找第一条 `type=message & role=assistant`，
把所有 `type=output_text` 的 text 拼起来。

为什么只读尾部
--------------
transcript 会长到十几 MB（本项目实测 18MB）。全文读一遍既慢又浪费，
倒着读最后几百 KB 足够覆盖最后一条回复。
"""

from __future__ import annotations

import json
import os
import re

# 单次最多往后读多少字节。一条回复 + 工具结果通常远小于这个值。
TAIL_BYTES = 600 * 1024

# 把 markdown 清成"适合朗读的纯文本"时，整行丢弃的前缀
_DROP_LINE = re.compile(r"^\s*(```|\||\$\s|\+{3}|-{3,}|={3,})")


def _iter_tail_lines(path: str, tail_bytes: int = TAIL_BYTES):
    """从文件尾部读，返回行列表（第一行可能是半截的被丢掉）。"""
    size = os.path.getsize(path)
    start = max(0, size - tail_bytes)
    with open(path, "rb") as f:
        f.seek(start)
        raw = f.read()
    text = raw.decode("utf-8", "replace")
    lines = text.splitlines()
    if start > 0 and lines:
        lines = lines[1:]          # 首行大概率被截断，丢弃
    return lines


def last_reply(transcript_path: str, tail_bytes: int = TAIL_BYTES) -> str:
    """返回最后一条 assistant 回复的原始 markdown；取不到就返回空串。"""
    if not transcript_path or not os.path.exists(transcript_path):
        return ""
    try:
        lines = _iter_tail_lines(transcript_path, tail_bytes)
    except Exception:
        return ""

    for line in reversed(lines):
        line = line.strip()
        if not line.startswith("{"):
            continue
        try:
            obj = json.loads(line)
        except Exception:
            continue
        if obj.get("type") != "message" or obj.get("role") != "assistant":
            continue
        parts = []
        content = obj.get("content")
        if isinstance(content, str):
            parts.append(content)
        elif isinstance(content, list):
            for blk in content:
                if isinstance(blk, dict) and blk.get("type") == "output_text":
                    t = blk.get("text")
                    if isinstance(t, str) and t.strip():
                        parts.append(t)
        if parts:
            return "\n".join(parts).strip()
    return ""


# ---------------------------------------------------------------- 清理 markdown

_LINK = re.compile(r"!?\[([^\]]*)\]\([^)]*\)")        # [文字](url) / ![alt](src)
_WIKI = re.compile(r"\[\[([^\]|]+)(?:\|([^\]]+))?\]\]")  # [[目标|显示]]
_BOLD = re.compile(r"\*{1,3}([^*]+)\*{1,3}")           # **粗** / *斜*
_CODE = re.compile(r"`([^`]*)`")                       # `code`
_EMOJI = re.compile(
    "[\U0001F300-\U0001FAFF\U00002600-\U000027BF\U0001F1E6-\U0001F1FF"
    "\u2190-\u21FF\u2B00-\u2BFF\uFE0F\u200D]+")
_HEAD = re.compile(r"^\s*#{1,6}\s*")
_BULLET = re.compile(r"^\s*(?:[-*+]|\d+[.)])\s+")
_QUOTE = re.compile(r"^\s*>+\s*")                      # 引用符不念
_SPACES = re.compile(r"[ \t]+")


def to_speech(md: str) -> str:
    """markdown → 适合朗读的纯文本。代码块和表格整段丢弃。"""
    if not md:
        return ""
    out: list[str] = []
    in_code = False
    for raw in md.splitlines():
        line = raw.rstrip()
        stripped = line.strip()

        if stripped.startswith("```"):
            in_code = not in_code
            continue
        if in_code:
            continue                     # 代码内容不念
        if not stripped:
            continue
        if _DROP_LINE.match(stripped):
            continue                     # 表格行、分隔线、数学块

        line = _HEAD.sub("", line)
        line = _BULLET.sub("", line)
        line = _QUOTE.sub("", line)
        line = _LINK.sub(lambda m: m.group(1), line)
        line = _WIKI.sub(lambda m: m.group(2) or m.group(1), line)
        line = _BOLD.sub(lambda m: m.group(1), line)
        line = _CODE.sub(lambda m: m.group(1), line)
        line = _EMOJI.sub("", line)
        line = _SPACES.sub(" ", line).strip()
        if line:
            out.append(line)
    return "\n".join(out)


_PUNCT_END = "。！？；…!?;"

# 回复里可以主动指定"念这一句"：`> 🎧 语音摘要：xxx`
# 有它就用它（上限放宽到 200 字，由写回复的人控制），
# 没有才退回"清理后开头 max_chars 字"。
# 这样播报内容由写回复的人决定，而不是被动截前两句——
# 后者常念到"我先看一下代码"这种零信息量的开场。
def _from_mark(md: str) -> str:
    """找 `> 🎧 语音摘要：…` 这类显式标记。

    逐行判断而不是一个复杂正则：标记行必须既有引用符、又含 🎧 或"语音摘要"，
    否则普通引用块（`> 注意…`）会被误当成播报内容。
    """
    for line in (md or "").splitlines():
        s = line.strip()
        if not s.startswith(">"):
            continue
        if "🎧" not in s and "语音摘要" not in s:
            continue
        s = s.lstrip(">").strip()
        s = re.sub(r"^🎧\s*", "", s)
        s = re.sub(r"^语音摘要\s*[：:]\s*", "", s)
        if s:
            return s
    return ""


def condense(text: str, max_chars: int = 120) -> str:
    """截到 max_chars 以内，并尽量停在句末，别把话说到一半。

    中文 TTS 约 4~5 字/秒，120 字≈25 秒。再长人就等不住了。
    """
    text = (text or "").strip()
    if not text:
        return ""
    # 压掉换行，读起来更连贯
    flat = re.sub(r"\s*\n\s*", " ", text)
    if len(flat) <= max_chars:
        return flat
    cut = flat[:max_chars]
    # 往回找一个句末标点，至少保留一半
    best = -1
    for i, ch in enumerate(cut):
        if ch in _PUNCT_END:
            best = i
    if best >= max_chars // 2:
        return cut[:best + 1]
    return cut.rstrip() + "…"


def last_reply_for_speech(transcript_path: str, max_chars: int = 120) -> str:
    """一步到位：transcript → 可朗读的短句。

    优先取回复里显式写的 `> 🎧 语音摘要：…`；
    没有才退回"清理后开头 max_chars 字"。
    """
    md = last_reply(transcript_path)
    if not md:
        return ""
    marked = _from_mark(md)
    if marked:
        return condense(to_speech(marked), max(max_chars, 200))
    return condense(to_speech(md), max_chars)
