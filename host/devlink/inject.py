"""把文字送进 WorkBuddy 的输入框。

三代做法的教训（都实测过）
--------------------------
1. 剪贴板 + Ctrl+V —— 窗口在前台 ≠ 焦点在输入框，粘贴经常落在页面空白处。
2. `SendInput` / `keybd_event` —— 本进程被 UIPI 隔离，函数返回 1 但事件根本没派发；
   而且 UIA 的 `SetFocus()` 只设无障碍焦点，不移动系统键盘焦点。
3. UIA `ValuePattern.SetValue` —— 能改 DOM，但**不触发 input 事件**，React 的受控状态
   不更新。表现就是：文字看得见，但按回车不发送、按退格删不掉，刷新一下才恢复。
   （用户实测反馈就是这个症状。）

现在的做法：往**渲染子窗口** `Chrome_RenderWidgetHostHWND` 发 `WM_CHAR`。
这是真实的字符输入路径，Chromium 会当作人打字处理，`input` 事件会触发，React 同步。
注意窗口必须是渲染子窗口，发给主窗口无效（Chromium 的编辑逻辑在 renderer 里）。

提交：UIA `Invoke` 点"发送"键。注意 WorkBuddy **正在回话时发送键不存在**
（那个位置换成"停止"），此时只能等——等它空闲再点。

清空：用退格字符 `WM_CHAR 0x08`，**不要**用 SetValue，否则 DOM 和 React 状态又分叉。
"""
from __future__ import annotations

import ctypes
import time
from ctypes import wintypes

DEFAULT_KEYWORD = "workbuddy"

# 输入框定位缓存：(hwnd, control, rect, buttons)。树遍历要 1~4s，缓存后基本为 0。
_CACHE: dict = {}

WM_KEYDOWN = 0x0100
WM_KEYUP = 0x0101
WM_CHAR = 0x0102
VK_BACK = 0x08

user32 = ctypes.windll.user32

SEND_NAMES = ("发送", "Send", "发送消息", "Send message")
STOP_HINTS = ("停止", "Stop")
AVOID_HINTS = ("语音", "Voice", "麦克风", "更多", "更多操作")

# 空输入框时 Chromium 会把 placeholder 文本当成 Value 返回（实测），
# 所以"值不是空串"不等于"里面有字"。判空必须排除这些。
PLACEHOLDER_HINTS = ("今天帮你做些什么", "引用对话文件", "调用技能与指令")


def is_blank_value(cur: str) -> bool:
    """判断输入框内容是否为空（含 placeholder 误报的情况）。"""
    s = (cur or "").replace("\ufeff", "").strip()
    if not s:
        return True
    return any(p in s for p in PLACEHOLDER_HINTS)


def _log(*a):
    import sys
    print(time.strftime("%H:%M:%S"), "[inject]", *a, file=sys.stderr, flush=True)


# ---------------------------------------------------------------- 窗口

def find_window(keyword: str = DEFAULT_KEYWORD) -> int | None:
    """按标题找可见主窗口，取面积最大的那个。"""
    import win32gui

    hits: list[tuple[int, int]] = []

    def cb(hwnd, _):
        if not win32gui.IsWindowVisible(hwnd):
            return
        title = win32gui.GetWindowText(hwnd)
        if title and keyword in title.lower():
            l, t, r, b = win32gui.GetWindowRect(hwnd)
            hits.append((max(0, r - l) * max(0, b - t), hwnd))

    try:
        win32gui.EnumWindows(cb, None)
    except Exception:
        return None
    if not hits:
        return None
    hits.sort(reverse=True)
    return hits[0][1]


def render_hwnd(keyword: str = DEFAULT_KEYWORD) -> int | None:
    """渲染子窗口。字符消息必须发给它，发给主窗口会被忽略。"""
    win = find_window(keyword)
    if not win:
        return None
    found = [None]

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(h, _l):
        buf = ctypes.create_unicode_buffer(256)
        user32.GetClassNameW(h, buf, 256)
        if "RenderWidgetHost" in buf.value:
            found[0] = h
            return False            # 找到即停
        return True

    try:
        user32.EnumChildWindows(win, cb, 0)
    except Exception:
        return None
    return found[0]


def focus_window(hwnd: int) -> bool:
    import win32gui

    try:
        if win32gui.IsIconic(hwnd):
            win32gui.ShowWindow(hwnd, 9)                  # SW_RESTORE
        try:
            win32gui.SetForegroundWindow(hwnd)
        except Exception:
            user32.keybd_event(0x12, 0, 0, 0)             # Alt
            try:
                win32gui.SetForegroundWindow(hwnd)
            finally:
                user32.keybd_event(0x12, 0, 2, 0)
        time.sleep(0.1)
        return True
    except Exception:
        return False


# ---------------------------------------------------------------- 控件树

def _walk(root, max_depth: int = 22, budget: int = 20000):
    import collections

    q = collections.deque([(root, 0)])
    seen = 0
    while q and seen < budget:
        node, d = q.popleft()
        seen += 1
        yield node, d
        if d >= max_depth:
            continue
        try:
            for k in node.GetChildren():
                q.append((k, d + 1))
        except Exception:
            continue


def scan(window=None, keyword: str = DEFAULT_KEYWORD, *, use_cache: bool = True):
    """一次遍历拿到「输入框 + 输入框那一行的按钮」。

    返回 (input_ctrl, rect, buttons) 或 (None, None, [])。
    """
    import uiautomation as auto

    hwnd = None
    if window is None:
        hwnd = find_window(keyword)
        if not hwnd:
            return None, None, []
        window = auto.ControlFromHandle(hwnd)
    else:
        try:
            hwnd = window.NativeWindowHandle
        except Exception:
            hwnd = 0

    if use_cache:
        hit = _CACHE.get(hwnd)
        # UIA 的控件引用是"活引用"：窗口不重建就一直有效。
        # 所以 TTL 给长一点（15s），只有校验失败才强制重扫——
        # 每次全树遍历要 1~3 秒，在等发送的循环里反复扫会把延迟拖到十几秒。
        if hit and time.time() - hit[0] < 15.0:
            ctrl, rect, buttons = hit[1], hit[2], hit[3]
            try:
                if ctrl.BoundingRectangle.width() > 0:
                    return ctrl, rect, buttons
            except Exception:
                pass
            _CACHE.pop(hwnd, None)

    try:
        wr = window.BoundingRectangle
    except Exception:
        return None, None, []

    floor = wr.top + wr.height() * 0.45
    best = None
    buttons: list = []
    for ctrl, _d in _walk(window):
        try:
            ct = ctrl.ControlTypeName
        except Exception:
            continue
        try:
            if ct == "EditControl":
                r = ctrl.BoundingRectangle
                if r.width() >= 240 and 28 <= r.height() <= 300 and r.top >= floor:
                    try:
                        ctrl.GetValuePattern()
                    except Exception:
                        continue
                    score = (r.top, r.width())
                    if best is None or score > best[0]:
                        best = (score, ctrl, r)
            elif ct == "ButtonControl" and best is not None:
                r = ctrl.BoundingRectangle
                br = best[2]
                if r.width() > 0 and br.bottom - 6 <= r.top <= br.bottom + 80:
                    buttons.append((ctrl, r))
        except Exception:
            continue

    if best is None:
        return None, None, []
    buttons.sort(key=lambda x: x[1].left)
    _CACHE[hwnd] = (time.time(), best[1], best[2], buttons)
    return best[1], best[2], buttons


def find_input(window=None, keyword: str = DEFAULT_KEYWORD, *, use_cache: bool = True):
    ctrl, rect, _b = scan(window, keyword, use_cache=use_cache)
    return ctrl, rect


def read_value(ed=None, keyword: str = DEFAULT_KEYWORD) -> str:
    """读输入框当前内容（只读，安全）。读不到返回空串。

    ⚠ "读不到"和"确实是空的"都返回 ""，需要区分时用 probe_value()。
    """
    ok, val = probe_value(ed, keyword)
    return val if ok else ""


def probe_value(ed=None, keyword: str = DEFAULT_KEYWORD, *,
                use_cache: bool = True) -> tuple[bool, str]:
    """返回 (是否真的读到了, 内容)。

    为什么要分开：WorkBuddy 被遮挡时无障碍树被销毁，读输入框会失败。
    这时把结果当成"空"会误判成"文字已发出"，必须让调用方知道"我不知道"。

    默认走缓存：输入框控件是活引用，定位一次就能一直读。
    强制全树遍历要 1~4 秒，而读值在等发送的循环里会被调用很多次。
    """
    if ed is None:
        ed, _r = find_input(keyword=keyword, use_cache=use_cache)
    if ed is None and use_cache:
        ed, _r = find_input(keyword=keyword, use_cache=False)
    if ed is None:
        return False, ""
    try:
        return True, ed.GetValuePattern().Value or ""
    except Exception:
        _CACHE.clear()
        return False, ""


# ---------------------------------------------------------------- 输入

def select_all(ed=None, keyword: str = DEFAULT_KEYWORD) -> bool:
    """选中输入框全部内容。之后插入的字符会**替换**选区，这是唯一可靠的"清空"手段。

    为什么不能靠退格：Chromium 删字符走键盘事件，而 `WM_CHAR` 只负责插字
    （`WM_CHAR 0x08` 实测无效）；`WM_KEYDOWN` 这类伪造按键又会被 Chromium 忽略。
    """
    if ed is None:
        ed, _r = find_input(keyword=keyword, use_cache=False)
    if ed is None:
        return False
    # 选区操作也要求控件有焦点，否则 select 完了插入也不会替换
    focus_input(ed, keyword)
    try:
        ed.GetTextPattern().DocumentRange.Select()
        return True
    except Exception as e:
        _log(f"选中失败：{type(e).__name__}")
        return False


def focus_input(ed=None, keyword: str = DEFAULT_KEYWORD) -> bool:
    """把焦点放进输入框。

    这一步不能省：Chromium 的 textarea **没有键盘焦点时不接受 WM_CHAR**
    （PostMessage 返回成功但字进不去，值一点不变）。
    之前几次"能输入"都是碰巧输入框处于聚焦状态。

    UIA 的 SetFocus 只设无障碍焦点，但 Chromium 响应它——
    实测调用后 HasKeyboardFocus 变 True，随后 WM_CHAR 就能落字。
    """
    if ed is None:
        ed, _r = find_input(keyword=keyword, use_cache=False)
    if ed is None:
        return False
    try:
        ed.SetFocus()
    except Exception as e:
        _log(f"SetFocus 失败：{type(e).__name__}")
        return False
    time.sleep(0.15)
    try:
        return bool(ed.HasKeyboardFocus)
    except Exception:
        return True


def type_text(text: str, keyword: str = DEFAULT_KEYWORD, *, delay: float = 0.012,
              ed=None) -> bool:
    """用 WM_CHAR 逐字输入（真实字符路径，React 能同步）。

    不要传换行：WorkBuddy 里回车就是"发送"，正文带换行会提前提交。
    """
    h = render_hwnd(keyword)
    if not h:
        _log("找不到渲染子窗口")
        return False
    # 没有焦点的话字符会被丢弃，且 PostMessage 仍返回成功（假成功）
    if not focus_input(ed, keyword):
        _log("输入框没拿到焦点")
        return False
    for ch in text:
        if ch in "\r\n":
            ch = " "
        if not user32.PostMessageW(h, WM_CHAR, ord(ch), 0):
            return False
        if delay:
            time.sleep(delay)
    return True


def replace_text(text: str, keyword: str = DEFAULT_KEYWORD,
                 delay: float = 0.012) -> bool:
    """替换输入框内容：先选中全部，再输入（输入会盖掉选区）。"""
    ed, _r = find_input(keyword=keyword, use_cache=False)
    if ed is None:
        return False
    if read_value(ed).strip():
        if not select_all(ed):
            return False
        time.sleep(0.08)
    return type_text(text, keyword, delay=delay)


def _send_button(btns):
    """空闲时发送键在"语音输入"右边；名字含 停止/语音/更多 的一律不要。"""
    voice_left = None
    for ctrl, r in btns:
        nm = (ctrl.Name or "").strip()
        if any(h in nm for h in AVOID_HINTS) or nm in STOP_HINTS:
            voice_left = r.left if voice_left is None else max(voice_left, r.left)
    for ctrl, r in btns:
        nm = (ctrl.Name or "").strip()
        if nm in SEND_NAMES:
            return ctrl
        if any(h in nm for h in AVOID_HINTS) or nm in STOP_HINTS:
            continue
        if voice_left is not None and r.left <= voice_left:
            continue
        if not nm:
            continue
        return ctrl
    return None


def is_busy(btns=None, keyword: str = DEFAULT_KEYWORD) -> bool:
    """WorkBuddy 正在回话（发送键被"停止"顶掉了）。"""
    if btns is None:
        _e, _r, btns = scan(keyword=keyword, use_cache=False)
    return any((c.Name or "").strip() in STOP_HINTS for c, _r in btns)


def press_send(keyword: str = DEFAULT_KEYWORD) -> bool:
    """点发送键。返回 True 只代表"点了"，要看输入框是否清空才算成功。

    速度很重要：这个函数在等空闲的循环里被反复调用，
    而整棵无障碍树遍历一次要 1~3 秒。所以优先用缓存（UIA 控件引用是活的，
    窗口不重建就一直有效），只有缓存里找不到发送键时才强制刷新一次。
    """
    _e, _r, btns = scan(keyword=keyword, use_cache=True)
    btn = _send_button(btns)
    if btn is None:
        _e, _r, btns = scan(keyword=keyword, use_cache=False)
        btn = _send_button(btns)
    if btn is None:
        return False
    try:
        btn.GetInvokePattern().Invoke()
        return True
    except Exception:
        _CACHE.clear()
        return False


# ---------------------------------------------------------------- 对外

def ensure_ready(keyword: str = DEFAULT_KEYWORD, timeout: float = 6.0) -> bool:
    """确保 WorkBuddy 可注入：必要时切到前台，等无障碍树重建。

    为什么必须这么做（实测）：
      Chromium 在**窗口被遮挡或处于后台时会销毁无障碍树**，
      `Chrome_RenderWidgetHostHWND` 子窗口句柄消失、UIA 一个 EditControl 都读不到。
      所以用户在浏览器/Excel 里干活时按 OK 说话，不切前台就永远注入不进去。
      切前台后约 0.5s 树就重建好了（render_hwnd 从 0x0 变成真实句柄）。

    副作用：会把 WorkBuddy 提到最前面。这是产品上必须接受的——说话就是为了发给它。
    """
    if render_hwnd(keyword) and find_input(keyword=keyword, use_cache=True)[0] is not None:
        return True

    hwnd = find_window(keyword)
    if not hwnd:
        return False
    _CACHE.clear()
    focus_window(hwnd)

    t0 = time.time()
    while time.time() - t0 < timeout:
        time.sleep(0.4)
        if render_hwnd(keyword) and find_input(keyword=keyword, use_cache=False)[0] is not None:
            _log(f"无障碍树已就绪（{(time.time() - t0) + 0.4:.1f}s）")
            return True
    _log(f"{timeout}s 内无障碍树仍未就绪")
    return False


def send_text(text: str, *, submit: bool = True, keyword: str = DEFAULT_KEYWORD,
              wait_busy: float = 15.0) -> str:
    """输入文字并（可选）发送。

    返回：
      ok                写入并已发送（输入框已空）
      wrote_no_submit   写进去了但没发出去（多半是 WorkBuddy 正在回话）
      no_window / no_input / write_failed / empty
    """
    if not text or not text.strip():
        return "empty"
    text = " ".join(text.split())

    hwnd = find_window(keyword)
    if not hwnd:
        return "no_window"

    # 关键：被遮挡时无障碍树是空的，必须先切前台等它重建
    if not ensure_ready(keyword):
        return "no_input"

    # ensure_ready 可能刚刚把窗口拉到前台、Chromium 重建了渲染子窗口，
    # 这时缓存里的控件引用指向的是旧窗口，GetValuePattern 读得到但 PostMessage 打不进去。
    # 所以这里必须强制重扫，不能吃缓存。
    _CACHE.clear()
    ed, _rect, btns = scan(keyword=keyword, use_cache=False)
    if ed is None:
        return "no_input"

    # 最多试两轮：第一轮没打进去就重新切前台、重扫、再打一次
    landed = False
    cur = ""
    for attempt in range(2):
        if attempt:
            _log("输入未落地，重试一次")
            _CACHE.clear()
            focus_window(hwnd)
            time.sleep(0.5)
            ed, _rect, _b = scan(keyword=keyword, use_cache=False)
            if ed is None:
                return "no_input"

        # 一律先选中全部。不能只在"看起来有内容"时才选：
        # React state 里可能留着旧文本而 DOM 已被 SetValue 清空（读起来是空的），
        # 这时直接输入会变成"旧+新"拼接，发出去内容就错了。
        select_all(ed)
        time.sleep(0.08)

        if not type_text(text, keyword, ed=ed):
            continue

        # 等文字真的出现在框里再发。
        # WM_CHAR 是 PostMessage（异步），Chromium 渲染和 ValuePattern 回读都有延迟，
        # 实测输入 24 个字后立刻读只看到 4 个字——一次读取就判失败会导致"永远发不出去"。
        # 而且必须校验**完全等于**待发文本，不能只判"包含"：
        # 若发生了追加，框里会是"旧文本+新文本"，只判包含照样通过，就发出错误内容。
        deadline = time.time() + 3.0
        while time.time() < deadline:
            time.sleep(0.15)
            ok, cur = probe_value()
            if not ok:
                continue
            if is_blank_value(cur):
                continue
            if cur.strip() == text.strip():
                landed = True
                break
        if landed:
            break

    if not landed:
        _log(f"输入校验失败，框内={(cur or '')[:40]!r} 期望={text[:40]!r}")
        # 内容不对就不能发：先中和掉，避免留下半截错误文本被误发
        clear_soft(keyword=keyword)
        return "write_failed"

    if not submit:
        return "ok"

    # 等空闲再点发送：回话期间根本没有发送键
    t0 = time.time()
    while True:
        if press_send(keyword):
            time.sleep(0.45)
            ok, cur = probe_value()
            # 判空要排除 placeholder，否则发出去了也会被当成"文字还在"
            if ok and is_blank_value(cur):
                return "ok"
        if time.time() - t0 >= wait_busy:
            return "wrote_no_submit"
        time.sleep(0.4)


def clear_soft(ed=None, keyword: str = DEFAULT_KEYWORD) -> bool:
    """安全清空：选中全部，输入一个空格。

    为什么不是 SetValue("")：SetValue 只改 DOM，**React state 仍留着旧文本**，
    之后任何一次重渲染都会把旧文字写回输入框。用户看到有字就按回车，
    于是同一条过期消息被反复发出去——实测就是这样连发了好几次。

    为什么要留一个空格：Chromium 只接受字符消息（WM_CHAR），伪造的
    WM_KEYDOWN/退格一律被忽略（有焦点也一样），所以删到真正的空串做不到。
    退而求其次：用 WM_CHAR 输入一个空格 —— 这样 React state 被同步成 " "，
    而 WorkBuddy 把纯空白视为空内容（发送键不出现），按回车也发不出去。
    残留因此完全无害。
    """
    if ed is None:
        ed, _r = find_input(keyword=keyword, use_cache=False)
    if ed is None:
        return False
    if is_blank_value(read_value(ed)):
        return True
    if not select_all(ed):
        return False
    time.sleep(0.08)
    type_text(" ", keyword, ed=ed, delay=0)
    # 读完是异步的，立刻读可能还是旧值；轮询 1.2s 再下结论
    deadline = time.time() + 1.2
    while time.time() < deadline:
        time.sleep(0.15)
        if is_blank_value(read_value(ed)):
            return True
    return False


def clear_hard(keyword: str = DEFAULT_KEYWORD) -> bool:
    """硬清空：SetValue("")。

    ⚠ 只用于"完全没有别的办法"的场合，并且**用完必须再 clear_soft 一次**
    把 React state 也拉回干净状态。单独使用会制造幽灵消息。
    """
    ed, _r = find_input(keyword=keyword, use_cache=False)
    if ed is None:
        return False
    try:
        ed.GetValuePattern().SetValue("")
        time.sleep(0.25)
        _CACHE.clear()
        return is_blank_value(read_value())
    except Exception:
        return False


def clear(keyword: str = DEFAULT_KEYWORD) -> bool:
    """默认清空走 clear_soft（React 会同步，不留幽灵）。"""
    return clear_soft(keyword=keyword)


def set_clipboard(text: str) -> bool:
    """兜底：把文字放剪贴板，用户可按 Ctrl+V。"""
    try:
        import win32clipboard
        import win32con

        win32clipboard.OpenClipboard()
        try:
            win32clipboard.EmptyClipboard()
            win32clipboard.SetClipboardData(win32con.CF_UNICODETEXT, text)
        finally:
            win32clipboard.CloseClipboard()
        return True
    except Exception:
        return False


# ---------------------------------------------------------------- 自检

def diagnose() -> dict:
    """不含副作用：报告能否注入。"""
    info: dict = {}
    hwnd = find_window()
    info["window"] = hex(hwnd) if hwnd else None
    if not hwnd:
        return info
    info["render_hwnd"] = hex(render_hwnd() or 0)
    ed, rect, btns = scan(use_cache=False)
    info["input"] = str(rect) if rect else None
    # render_hwnd 或 input 缺一，说明窗口被遮挡、无障碍树已被 Chromium 销毁，
    # 这时注入前必须先切前台（send_text 会自动做）。
    info["ready"] = bool(render_hwnd() and ed is not None)
    if not info["ready"]:
        info["note"] = "窗口被遮挡，无障碍树不可用；注入时会自动切前台"
        return info
    if ed is not None:
        info["value"] = read_value(ed)[:40]
        info["buttons"] = [(c.Name or "", r.left) for c, r in btns]
        info["busy"] = is_busy(btns)
        info["send_button"] = bool(_send_button(btns))
    return info
