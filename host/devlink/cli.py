"""命令行：不用打开 WorkBuddy 也能验硬件。

    python -m devlink.cli doctor        # 体检：串口/固件/服务/依赖
    python -m devlink.cli say "你好"     # 让设备说话
    python -m devlink.cli status thinking "跑着呢"
    python -m devlink.cli reply "一行字"
    python -m devlink.cli sessions      # 多会话时 ①②③ 分别是哪个会话
    python -m devlink.cli ask "继续吗"   # 到设备上按 OK / ↓
    python -m devlink.cli screen        # 画一屏中文，检查字形
    python -m devlink.cli mic 5         # 按住 OK 说话，识别后注入 WorkBuddy
    python -m devlink.cli rec 5         # 只录音+识别+存盘，不碰输入框（诊断用）
    python -m devlink.cli play raw      # 播放上次录音，判断是没录到还是识别不准
    python -m devlink.cli play clean    # 播放增强后的
    python -m devlink.cli micprobe      # 麦克风体检：扫增益、报电平
    python -m devlink.cli setup         # 一键写 hooks + MCP 配置（带备份、幂等）
    python -m devlink.cli unsetup       # 撤掉上面写的配置
    python -m devlink.cli install       # 装开机自启
    python -m devlink.cli uninstall

⚠ 两个"清"完全不同，别搞混（我踩过这个坑）：
    clear   清**设备屏幕**上的文字
    purge   清 **WorkBuddy 输入框**里的残留（会同步 React 状态）
"""
from __future__ import annotations

import os
import sys
import time

from .server import PORT, _client, running

HERE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STARTUP = os.path.join(os.environ.get("APPDATA", ""), "Microsoft", "Windows",
                       "Start Menu", "Programs", "Startup", "wb-daemon.cmd")


def need_service() -> bool:
    if running():
        return True
    print("服务没在跑。先执行：\n"
          f'  "{sys.executable}" -m devlink.server', file=sys.stderr)
    return False


def cmd_inject() -> int:
    """自检注入链路：定位 → 输入（WM_CHAR）→ 点发送 → 校验输入框已清空。"""
    from . import inject

    info = inject.diagnose()
    print("窗口：      ", info.get("window"))
    print("可注入：    ", info.get("ready"))
    if info.get("note"):
        print("说明：      ", info["note"])
    if info.get("ready"):
        print("渲染窗口：  ", info.get("render_hwnd"))
        print("输入框：    ", info.get("input"))
        print("当前内容：  ", repr(info.get("value")))
        print("底部按钮：  ", info.get("buttons"))
        print("正在回话：  ", info.get("busy"))
        print("发送键可用：", info.get("send_button"))
    if not info.get("window"):
        print("找不到 WorkBuddy 窗口——先打开它")
        return 1
    text = "".join(sys.argv[2:]) or "注入自检：这条由硬件链路送进来"
    print("\n写入：", text)
    r = inject.send_text(text)
    print("结果：", r)
    if r == "ok":
        print("✅ 已写入并发送（输入框已清空）")
        return 0
    if r == "wrote_no_submit":
        print("⚠ 文字已写入输入框，但发送键此刻不存在（WorkBuddy 正在回话）")
        print("  服务会自动等它空闲再点发送；手动按回车也可以")
        return 1
    if r == "no_input":
        print("❌ 切前台后仍然读不到输入框（无障碍树没恢复）")
        return 1
    print("❌ 失败：", r)
    return 1


def cmd_purge() -> int:
    """清 WorkBuddy 输入框里的残留，并同步 React 状态。

    和 clear 的区别（很重要）：clear 清的是**设备屏幕**，
    这个清的是 **WorkBuddy 输入框**。
    用 SetValue 清 DOM 是不够的——React state 会留着原文，
    之后按一次回车就把旧句子发出去（实测连发多次）。
    所以这里走 WM_CHAR：选中全部 + 输入一个空格，让 state 也跟着变成空白。
    """
    from . import inject

    print("切前台、等无障碍树…")
    if not inject.ensure_ready():
        print("❌ WorkBuddy 的无障碍树没就绪（窗口被遮挡超时）")
        return 1

    ok, dom = inject.probe_value()
    print(f"清理前 DOM: {dom[:50]!r}")

    done = inject.clear_soft()
    time.sleep(0.4)
    ok2, dom2 = inject.probe_value()
    _e, _r, btns = inject.scan(use_cache=False)
    has_send = inject._send_button(btns) is not None

    print(f"清理后 DOM: {dom2[:50]!r}")
    print(f"发送键: {'有（回车能发出去）' if has_send else '无（回车发不出去）'}")
    if done and inject.is_blank_value(dom2) and not has_send:
        print("✅ 已清空，且按回车也发不出东西")
        return 0
    print("⚠ 仍有残留，请手动在 WorkBuddy 里删一下")
    return 1


def cmd_log(n: int = 40) -> int:
    """打印最近的设备事件日志（按键、录音、识别、注入）。"""
    from .terminal import LOG_PATH

    if not os.path.exists(LOG_PATH):
        print("还没有事件日志：", LOG_PATH)
        return 1
    with open(LOG_PATH, encoding="utf-8", errors="replace") as f:
        lines = f.read().splitlines()
    print(f"事件日志：{LOG_PATH}（共 {len(lines)} 行，显示最后 {n} 行）\n")
    for line in lines[-n:]:
        print("  " + line)
    return 0


def cmd_speaklast(transcript: str = "") -> int:
    """手动触发"念上一轮结论"，用来验音质。

    不给路径就自动找当前工作目录对应的最新 transcript。
    给 `--dry` 只打印会被念的内容，不真的出声。
    """
    from . import digest

    if not transcript:
        # ~/.workbuddy/projects/<cwd 转换>/<session>.jsonl
        import glob

        base = os.path.join(os.path.expanduser("~"), ".workbuddy", "projects")
        files = glob.glob(os.path.join(base, "**", "*.jsonl"), recursive=True)
        if not files:
            print("找不到 transcript：", base)
            return 1
        transcript = max(files, key=os.path.getmtime)
    if not os.path.exists(transcript):
        print("文件不存在：", transcript)
        return 1

    text = digest.last_reply_for_speech(transcript, 120)
    raw = digest.last_reply(transcript)
    print(f"transcript: {transcript}")
    print(f"最后一条回复 {len(raw)} 字 → 可念 {len(text)} 字")
    print()
    print("会念：", text if text else "(空)")
    if not text:
        return 1
    if "--dry" in sys.argv:
        print("\n（--dry：不真的播报）")
        return 0

    if not need_service():
        return 2
    r = _client({"op": "speak_last", "transcript_path": transcript,
                 "max_chars": 120}, 30)
    print("\n已排队播报：", r.get("spoken", "")[:60])
    print("大约", max(3, len(text) // 5), "秒后念完")
    return 0 if r.get("ok") else 1


def cmd_doctor() -> int:
    print("== AI PASSPORT 体检 ==")
    ok = True

    from . import codec, link, speech  # noqa: F401

    # 1) 依赖
    deps = ["serial", "numpy", "win32com.client", "mcp"]
    for d in deps:
        try:
            __import__(d)
            print(f"  依赖 {d:16s} OK")
        except Exception as e:
            print(f"  依赖 {d:16s} 缺（{type(e).__name__}）")
            ok = False

    # 2) 串口
    port = link.find_device_port()
    print(f"  设备串口          {port or '未找到（插好数据线）'}")
    ok = ok and bool(port)

    # 3) 服务
    up = running()
    print(f"  后台服务          {'运行中 :%d' % PORT if up else '未运行'}")

    # 4) 若服务在跑，问一下固件
    if up:
        info = _client({"op": "ping"}, 5)
        print(f"  固件              {info.get('fw') or '?'}   串口={info.get('port')}")
        ok = ok and bool(info.get("ok"))

    # 5) 字体
    font = os.path.join(HERE, "..", "main", "fonts", "lv_font_wb_zh_16.c")
    print(f"  中文字库          {'已生成' if os.path.exists(font) else '缺（python tools/gen_font.py）'}")

    print("==> ", "全部就绪" if ok else "有项目未就绪")
    return 0 if ok else 1


def cmd_screen() -> int:
    """推一屏内容，专门用来肉眼确认字形有没有方块。"""
    if not need_service():
        return 2
    lines = [
        "就绪 思考中 待确认 有提醒",
        "确认 / 拒绝 / 说话 / 聆听",
        "串口 伴侣 播报 缓冲 静音",
        "允许执行命令？读取文件路径",
        "↑↓ 翻看  按住 OK 说话",
        "0123456789 ABC xyz ！？，。",
    ]
    _client({"op": "clear"}, 5)
    _client({"op": "status", "state": "idle", "text": "字形自检 中文正常"}, 5)
    for ln in lines:
        _client({"op": "reply", "text": ln}, 5)
    print("已推送。看设备屏幕：应该全是正常汉字，没有空心方块。")
    return 0


def cmd_setup() -> int:
    """把 hooks 与 MCP 配置写进 WorkBuddy（真正的一键接入）。

    为什么需要它：`install` 只装开机自启，hooks / mcp.json 原本得手改，
    而手改正是最容易出错、也最劝退别人的一步。

    三条安全底线：
      1. **先备份**：改前把原文件复制成 .bak.<时间戳>，随时可还原
      2. **幂等**：已有同一条命令就跳过，重复跑不会叠加出重复 hook
      3. **只合并不覆盖**：用户自己的其它 hook / MCP server 一个字都不动
    """
    import json
    import shutil

    node = shutil.which("node")
    if not node:
        # 退而求其次：WorkBuddy 自带的 node
        cand = os.path.expanduser(
            "~/.workbuddy/binaries/node/versions/22.22.2-3/node.exe")
        if os.path.exists(cand):
            node = cand
    if not node:
        print("找不到 node。先装 Node.js（hook 要用它做转发，冷启动 0.28s），\n"
              "或告诉我在哪：WB_NODE=<node 路径>", file=sys.stderr)
        return 2
    node = node.replace("\\", "/")

    hook_js = os.path.join(HERE, "hook.js").replace("\\", "/")
    mcp_py = os.path.join(HERE, "mcp_server.py").replace("\\", "/")
    pyexe = sys.executable.replace("\\", "/")

    home = os.path.expanduser("~/.workbuddy")
    os.makedirs(home, exist_ok=True)
    set_path = os.path.join(home, "settings.json")
    mcp_path = os.path.join(home, "mcp.json")

    def load(p):
        if not os.path.exists(p):
            return {}
        try:
            with open(p, encoding="utf-8") as f:
                return json.load(f)
        except (ValueError, OSError) as e:
            print(f"读不了 {p}：{e}", file=sys.stderr)
            return None

    def backup(p):
        if not os.path.exists(p):
            return ""
        b = f"{p}.bak.{time.strftime('%Y%m%d-%H%M%S')}"
        shutil.copy2(p, b)
        return b

    settings = load(set_path)
    mcp = load(mcp_path)
    if settings is None or mcp is None:
        return 2

    # ---------------- hooks ----------------
    def cmd_for(event):
        return f'"{node}" "{hook_js}" {event}'

    # (事件, matcher, timeout)。PreToolUse 的 timeout 要够长：它要等人在设备上按键
    specs = [
        ("SessionStart", "startup|resume", 8),
        ("UserPromptSubmit", None, 8),
        ("Notification", "permission_prompt|idle_prompt", 8),
        ("Stop", None, 8),
        ("PreToolUse", "*", 120),
    ]

    hooks = settings.setdefault("hooks", {})
    added, updated, dup = [], [], []

    # 判断一条 hook 是不是"我们写的"：不看完整字符串。
    # 曾经用完整 command 比较，结果 node.exe / node.EXE 大小写差一个字母
    # 就被判成"不存在"，于是重复添加，每个事件触发两次。
    # 正确做法是看它引用的脚本是不是本仓库的 hook.js。
    hook_js_norm = hook_js.lower()

    def is_ours(cmd: str) -> bool:
        return hook_js_norm in str(cmd).replace("\\", "/").lower()

    for event, matcher, timeout in specs:
        arr = hooks.setdefault(event, [])
        want = cmd_for(event)

        # 先把该事件下所有"我们的"旧条目找出来；保留第一条并更新命令，其余删除
        found = []
        for grp in arr:
            if not isinstance(grp, dict):
                continue
            for h in (grp.get("hooks") or []):
                if isinstance(h, dict) and is_ours(h.get("command", "")):
                    found.append((grp, h))

        if not found:
            entry = {"hooks": [{"type": "command", "command": want, "timeout": timeout}]}
            if matcher:
                entry["matcher"] = matcher
            arr.append(entry)
            added.append(event)
            continue

        keep_grp, keep_h = found[0]
        if keep_h.get("command") != want or keep_h.get("timeout") != timeout:
            keep_h["command"] = want
            keep_h["timeout"] = timeout
            updated.append(event)
        if matcher:
            keep_grp["matcher"] = matcher

        # 清掉重复条目（同一事件里多余的"我们的"hook）
        for grp, h in found[1:]:
            try:
                grp["hooks"].remove(h)
            except ValueError:
                pass
            dup.append(event)
        # 顺手清掉被掏空的 group
        for grp in list(arr):
            if isinstance(grp, dict) and grp.get("hooks") == []:
                arr.remove(grp)

    # ---------------- MCP ----------------
    servers = mcp.setdefault("mcpServers", {})
    want_srv = {
        "command": pyexe,
        "args": [mcp_py],
        "env": {"WB_DAEMON_PORT": str(PORT), "WB_ASR_BACKEND": "sense"},
    }
    if servers.get("ai-passport-companion") == want_srv:
        mcp_state = "已是最新"
    else:
        servers["ai-passport-companion"] = want_srv
        mcp_state = "已写入"

    # ---------------- 落盘 ----------------
    # 只在内容真的变了才备份+写。否则幂等跑一次就多一个 .bak，堆积成垃圾。
    def dump(obj):
        return json.dumps(obj, ensure_ascii=False, indent=2, sort_keys=True)

    b1 = b2 = ""
    changed = []
    on_disk_s = dump(load(set_path) or {})
    on_disk_m = dump(load(mcp_path) or {})
    if dump(settings) != on_disk_s:
        b1 = backup(set_path)
        with open(set_path, "w", encoding="utf-8") as f:
            f.write(dump(settings))
        changed.append("settings.json")
    if dump(mcp) != on_disk_m:
        b2 = backup(mcp_path)
        with open(mcp_path, "w", encoding="utf-8") as f:
            f.write(dump(mcp))
        changed.append("mcp.json")

    print(f"node      : {node}")
    print(f"settings  : {set_path}")
    print(f"  hooks 新增: {', '.join(added) if added else '(无)'}")
    print(f"  hooks 更新: {', '.join(updated) if updated else '(无)'}")
    if dup:
        print(f"  hooks 去重: {', '.join(dup)}（发现重复条目，已清除）")
    print(f"mcp.json  : {mcp_path}  → ai-passport-companion {mcp_state}")
    if not changed:
        print("  无需改动，配置已是最新（未产生备份文件）")
    for b in (b1, b2):
        if b:
            print(f"  备份: {b}")
    print()
    print("下一步：")
    print(f'  "{pyexe}" -m devlink.cli install     # 开机自启')
    print(f'  "{pyexe}" -m devlink.server          # 或现在手动起服务')
    print()
    print("注意：MCP 新增项需要在 WorkBuddy 的连接器页面点一次「信任」才生效。")
    return 0


def cmd_setup_remove() -> int:
    """撤掉本项目的 hooks 与 MCP 条目，其余配置不动。"""
    import json
    import shutil

    home = os.path.expanduser("~/.workbuddy")
    set_path = os.path.join(home, "settings.json")
    mcp_path = os.path.join(home, "mcp.json")
    hook_js = os.path.join(HERE, "hook.js").replace("\\", "/")

    def backup(p):
        if os.path.exists(p):
            b = f"{p}.bak.{time.strftime('%Y%m%d-%H%M%S')}"
            shutil.copy2(p, b)
            return b
        return ""

    n_hook = n_mcp = 0
    if os.path.exists(set_path):
        d = json.load(open(set_path, encoding="utf-8"))
        hooks = d.get("hooks") or {}
        for event in list(hooks):
            kept = []
            for grp in hooks[event]:
                if not isinstance(grp, dict):
                    kept.append(grp)
                    continue
                hs = [h for h in (grp.get("hooks") or [])
                      if not (isinstance(h, dict) and hook_js in str(h.get("command", "")))]
                if len(hs) != len(grp.get("hooks") or []):
                    n_hook += len(grp.get("hooks") or []) - len(hs)
                    if hs:
                        grp["hooks"] = hs
                        kept.append(grp)
                    continue
                kept.append(grp)
            if kept:
                hooks[event] = kept
            else:
                hooks.pop(event, None)
        if not hooks:
            d.pop("hooks", None)
        backup(set_path)
        with open(set_path, "w", encoding="utf-8") as f:
            json.dump(d, f, ensure_ascii=False, indent=2)

    if os.path.exists(mcp_path):
        d = json.load(open(mcp_path, encoding="utf-8"))
        srv = d.get("mcpServers") or {}
        if "ai-passport-companion" in srv:
            srv.pop("ai-passport-companion")
            n_mcp = 1
            backup(mcp_path)
            with open(mcp_path, "w", encoding="utf-8") as f:
                json.dump(d, f, ensure_ascii=False, indent=2)

    print(f"已移除 hooks {n_hook} 条、MCP 条目 {n_mcp} 个。原文件已备份。")
    return 0


def cmd_install() -> int:
    os.makedirs(os.path.dirname(STARTUP), exist_ok=True)
    body = ("@echo off\r\n"
            "rem WorkBuddy 硬件伴侣后台服务（删掉本文件即取消自启）\r\n"
            f'start "" /min "{sys.executable}" -m devlink.server\r\n')
    with open(STARTUP, "w", encoding="utf-8", newline="") as f:
        f.write(body)
    print("已安装开机自启：", STARTUP)
    return 0


def cmd_uninstall() -> int:
    try:
        os.remove(STARTUP)
        print("已移除开机自启")
    except FileNotFoundError:
        print("本来就没装")
    return 0


def cmd_mic(sec: int) -> int:
    if not need_service():
        return 2
    print(f"请按住设备 OK 说一句话（{sec} 秒内）…")
    r = _client({"op": "listen", "timeout": sec}, sec + 10)
    print("识别结果：", r.get("text") or r.get("error", "无"))
    return 0


def cmd_rec(sec: int = 5) -> int:
    """录音 + 识别 + 存盘，并打印电平诊断。

    和 mic 的区别：mic 走"按键说话"路径（会真的注入 WorkBuddy），
    rec 只录音和识别，**绝不碰输入框**，用来单独判断麦克风与识别质量。
    """
    print(f"录 {sec} 秒，请对着设备说话…")
    r = _client({"op": "record", "sec": sec, "asr": True}, sec + 40)
    if not r.get("ok"):
        print("失败：", r.get("error"))
        return 1
    st = r.get("stats") or {}
    print(f"  采样 {r.get('samples')}（{r.get('sec')}s）")
    print(f"  电平 ac_rms={st.get('ac_rms')} peak={st.get('peak')} "
          f"clean_rms={st.get('clean_rms')}")
    print(f"  识别：{r.get('text')!r}")
    for k in ("raw_wav", "clean_wav"):
        if st.get(k):
            print(f"  {k}: {st[k]}")
    print()
    print("  听一下录到了什么： python -m devlink.cli play raw")
    print("  听增强后的：       python -m devlink.cli play clean")
    return 0


def cmd_play(which: str = "raw") -> int:
    """播放上次录音。用来判断是"没录到"还是"识别不准"。

    raw   = 诊断录音的原始波形（cli rec 产生）
    clean = 诊断录音增强后
    voice = **上一次真实语音**（按住 OK 说的那句，已切掉开头爆音）
    """
    w = (which or "raw").lower()
    if w.startswith("v"):
        name = "last_voice.wav"
    elif w.startswith("c"):
        name = "last_clean.wav"
    else:
        name = "last_raw.wav"
    p = os.path.join(HERE, "recordings", name)
    if not os.path.exists(p):
        print(f"没有录音文件：{p}")
        print("raw/clean 需要先跑 cli rec 5；voice 需要先按住 OK 说一句话")
        return 1
    import wave

    try:
        with wave.open(p, "rb") as f:
            sec = f.getnframes() / f.getframerate()
        print(f"播放 {p}（{sec:.1f}s）")
    except Exception:
        print(f"播放 {p}")
    import winsound
    try:
        winsound.PlaySound(p, winsound.SND_FILENAME)
        return 0
    except Exception as e:
        print(f"播放失败：{e}")
        return 1


def cmd_micprobe() -> int:
    """麦克风体检：扫增益、报电平，判断是"没信号"还是"削顶"。

    用法：先跑它，然后对着卡片连续说话（不要停）。
    """
    if not need_service():
        return 2
    print("对着卡片持续说话（正常音量，距离 20cm 左右），下面逐档测量…\n")
    print(" 增益   原始RMS   峰值   AC-RMS  判定")
    best = None
    for db in (30.0, 27.0, 24.0, 21.0, 18.0, 15.0, 12.0, 9.0):
        _client({"op": "gain", "db": db}, 8)
        r = _client({"op": "record", "sec": 2, "asr": False}, 30)
        st = r.get("stats") or {}
        rms, peak = st.get("raw_rms", 0), st.get("peak", 0)
        if rms < 60:
            verdict = "无信号（麦克风通路没通）"
        elif peak >= 32000:
            verdict = "削顶（增益太大）"
        else:
            verdict = "可用 ✅"
            if best is None or rms > best[1]:
                best = (db, rms)
        print(f"{db:5.1f}  {rms:8}  {peak:6}  {st.get('ac_rms', 0):6}  {verdict}")
    print()
    if best:
        db, rms = best
        print(f"建议增益：{db:.0f} dB（RMS {rms}）")
        print(f"  固定下来： python -m devlink.cli gain {db:.0f}")
    else:
        print("各档都拿不到有效电平 —— 这不是软件能修的：")
        print("  · 确认卡片上有麦克风开孔，且没被贴膜/外壳挡住")
        print("  · 对照厂商 audio demo（OK 播 1kHz、UP 录 3 秒回放）是否能录到声音")
        print("  · 若 demo 也录不到，属于硬件 bring-up 问题")
    _client({"op": "gain", "db": 15}, 8)
    return 0


def main() -> int:
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 0
    cmd, *rest = args

    if cmd == "doctor":
        return cmd_doctor()
    if cmd == "inject":
        return cmd_inject()
    if cmd == "install":
        return cmd_install()
    if cmd == "uninstall":
        return cmd_uninstall()
    if cmd == "setup":
        return cmd_setup()
    if cmd == "unsetup":
        return cmd_setup_remove()
    if cmd == "screen":
        return cmd_screen()
    if cmd == "purge":
        return cmd_purge()
    if cmd == "log":
        return cmd_log(int(rest[0]) if rest else 40)
    if cmd == "speaklast":
        # 第一个不以 - 开头的参数当 transcript 路径，其余是开关
        arg = next((a for a in rest if not a.startswith("-")), "")
        return cmd_speaklast(arg)

    if not need_service():
        return 2

    if cmd == "sessions":
        r = _client({"op": "sessions"}, 5)
        rows = r.get("sessions") or []
        if not rows:
            print("当前没有活跃会话（或服务刚启动）。")
            return 0
        if len(rows) == 1:
            print("只有 1 个活跃会话，屏幕上不加前缀。")
        print(f"{'标签':4s} {'会话':10s} {'空闲':>6s}  标题")
        for x in rows:
            print(f"{x['tag']:4s} {x['sid']:10s} {x['idle']:5d}s  {x['title']}")
        return 0
    if cmd == "forget":
        _client({"op": "forget"}, 5)
        print("已清空会话标签表。")
        return 0

    if cmd == "say":
        r = _client({"op": "speak", "text": " ".join(rest)}, 180)
        print("播报：", r.get("result"))
        return 0
    if cmd == "status":
        state = rest[0] if rest else "idle"
        text = " ".join(rest[1:])
        _client({"op": "status", "state": state, "text": text}, 5)
        return 0
    if cmd == "reply":
        _client({"op": "reply", "text": " ".join(rest)}, 5)
        return 0
    if cmd == "clear":
        _client({"op": "clear"}, 5)
        return 0
    if cmd == "ask":
        t = 60
        if len(rest) > 1 and rest[-1].isdigit():
            t = int(rest.pop())
        print("等待按键…")
        r = _client({"op": "ask", "text": " ".join(rest) or "确认？", "timeout": t}, t + 10)
        print("回答：", r.get("answer"))
        return 0
    if cmd == "type":
        r = _client({"op": "type", "text": " ".join(rest), "submit": True}, 20)
        print("注入：", r.get("result"))
        return 0
    if cmd == "mic":
        return cmd_mic(int(rest[0]) if rest else 8)
    if cmd == "rec":
        return cmd_rec(int(rest[0]) if rest else 5)
    if cmd == "play":
        return cmd_play(rest[0] if rest else "raw")
    if cmd == "micprobe":
        return cmd_micprobe()
    if cmd == "gain":
        _client({"op": "gain", "db": float(rest[0]) if rest else 15}, 8)
        print("增益已设为", rest[0] if rest else 15, "dB")
        return 0

    print("未知命令：", cmd)
    print(__doc__)
    return 1


if __name__ == "__main__":
    sys.exit(main())
