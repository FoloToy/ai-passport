"""多会话辨识：给每个 WorkBuddy 会话一个稳定短标签，标在屏幕消息前面。

## 为什么需要

WorkBuddy 是**单窗口多标签**结构（实测：一个顶层窗口只含一个
`Chrome_RenderWidgetHostHWND`），所有会话共用同一个输入框 DOM。
硬件注入天然落到"眼前那个标签"，所以**不需要判断发到哪个窗口**。

真正的麻烦在反方向：hook 事件来自所有会话，全挤到同一块 240x320 屏幕上，
交替覆盖，屏幕上看不出哪条属于哪个会话。`hook.js` 原本连 `session_id`
都没往上传（引用次数 0），等于信息全丢。

## 设计取舍

1. **单会话时零噪音**：只有一个会话活跃时不加任何前缀，屏幕和以前一模一样。
   只有第 2 个会话出现才启用标记——日常单人使用不该被标记污染。
2. **标签用带圈数字**（①②③）：字库已确认覆盖（tools/check_font_cov.py，
   8058 码点含 ①-⑩），且**只占 1 个汉字宽度**。屏幕一行只有 13 个汉字，
   用 `[2] ` 会吃掉 2 个字，用 `②` 只吃 1 个。
3. **标签必须稳定**：按首次出现顺序分配，不因活跃度重排。否则同一个会话
   的编号会跳，标签就失去意义。
4. **空闲释放**：30 分钟没动静的会话交还标签，避免 238 个历史会话把标签池
   占满。实测这台机器有 238 个历史 transcript，不释放会立刻耗尽。
5. **切换时清屏**：从单会话进入多会话时，历史行的归属无从判断，直接清屏并
   告知。宁可丢历史，也不显示带错误归属的内容——这正是本项目一贯的
   「不假装成功」原则。
"""
from __future__ import annotations

import json
import os
import time

# 带圈数字 ①-⑩，字库已确认覆盖。只占 1 个汉字宽度，是屏幕上最省的标记。
TAGS = "①②③④⑤⑥⑦⑧⑨⑩"

# 30 分钟无活动就释放标签。比起"永久占用"，这个时长足够覆盖一次连续工作。
IDLE_RELEASE_SEC = 30 * 60


class SessionTags:
    """会话 → 短标签 + 标题的映射表。"""

    def __init__(self, idle_release: float = IDLE_RELEASE_SEC) -> None:
        self.idle_release = idle_release
        self._tag: dict[str, int] = {}        # sid -> 标签索引 (0-based)
        self._seen: dict[str, float] = {}     # sid -> 最后活跃时间
        self._title: dict[str, str] = {}      # sid -> aiTitle（懒加载）
        self._title_tried: set[str] = set()   # 已尝试读过标题的 sid
        self._multi = False                   # 是否已进入多会话模式

    # ------------------------------------------------------------ 内部

    def _reap(self) -> None:
        """回收空闲会话，归还标签。"""
        now = time.time()
        for sid, ts in list(self._seen.items()):
            if now - ts > self.idle_release:
                del self._seen[sid]
                self._tag.pop(sid, None)
                self._title.pop(sid, None)
                self._title_tried.discard(sid)
        self._multi = len(self._tag) > 1

    def _alloc(self) -> int:
        used = set(self._tag.values())
        for i in range(len(TAGS)):
            if i not in used:
                return i
        # 超过 10 个活跃会话：抢最久没动静的那个（极端情况，可接受）
        oldest = min(self._seen.items(), key=lambda kv: kv[1])[0]
        i = self._tag.get(oldest, 0)
        self._tag.pop(oldest, None)
        self._seen.pop(oldest, None)
        return i

    # ------------------------------------------------------------ 标题

    def load_title(self, sid: str, transcript_path: str) -> str:
        """从 transcript 头部读 ai-title。

        WorkBuddy 会在 transcript 前几行写一条
        `{"type":"ai-title","aiTitle":"为 AI PASSPORT 开发..."}`。
        只读前 ~40 行，避免碰 18MB 的大文件。
        """
        if sid in self._title or sid in self._title_tried:
            return self._title.get(sid, "")
        self._title_tried.add(sid)
        if not transcript_path or not os.path.exists(transcript_path):
            return ""
        try:
            with open(transcript_path, encoding="utf-8", errors="replace") as f:
                for i, line in enumerate(f):
                    if i > 40:
                        break
                    if "aiTitle" not in line:
                        continue
                    try:
                        obj = json.loads(line)
                    except ValueError:
                        continue
                    if obj.get("type") == "ai-title":
                        t = str(obj.get("aiTitle") or "").strip()
                        if t:
                            self._title[sid] = t
                            return t
        except OSError:
            pass
        return ""

    # ------------------------------------------------------------ 对外

    def note(self, sid: str) -> bool:
        """登记一次活动。返回 True 表示"刚刚从单会话进入多会话"。"""
        if not sid:
            return False
        self._reap()
        was_multi = self._multi
        self._seen[sid] = time.time()
        if sid not in self._tag:
            self._tag[sid] = self._alloc()
        self._multi = len(self._tag) > 1
        return self._multi and not was_multi

    def label(self, sid: str) -> str:
        """返回形如 `②` 的前缀；空字符串表示不需要（无会话 / 单会话）。"""
        if not sid:
            return ""
        self._reap()
        if len(self._tag) <= 1:
            return ""
        i = self._tag.get(sid)
        return TAGS[i] if i is not None and i < len(TAGS) else ""

    def tag_text(self, sid: str, text: str) -> str:
        """给一段对话文本加上会话前缀。单会话时原样返回。"""
        lb = self.label(sid)
        return f"{lb}{text}" if lb else text

    def header(self, sid: str = "") -> str:
        """标题栏文本。单会话保持原样，多会话显示标签 + 会话数。"""
        self._reap()
        if len(self._tag) <= 1:
            return "WorkBuddy"
        lb = self.label(sid) if sid else ""
        return f"WB{lb} {len(self._tag)}会话" if lb else f"WB {len(self._tag)}会话"

    def describe(self) -> list[dict]:
        """给 `cli sessions` 用：当前认得的会话及其标签、标题、空闲时长。"""
        self._reap()
        now = time.time()
        out = []
        for sid, i in sorted(self._tag.items(), key=lambda kv: kv[1]):
            out.append({
                "tag": TAGS[i] if i < len(TAGS) else "?",
                "sid": sid[:8],
                "title": self._title.get(sid, "") or "(未命名)",
                "idle": int(now - self._seen.get(sid, now)),
            })
        return out

    def reset(self) -> None:
        self._tag.clear()
        self._seen.clear()
        self._title.clear()
        self._title_tried.clear()
        self._multi = False


# 进程级单例：整个服务共享一张表
TAGS_STATE = SessionTags()
