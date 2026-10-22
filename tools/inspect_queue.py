"""只读检查 WorkBuddy 输入框上方的待发队列，不点任何按钮。"""
import sys, time, os

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, os.path.join(ROOT, "host"))

from devlink import inject
import uiautomation as auto


def main():
    inject.ensure_ready()
    hwnd = inject.find_window()
    win = auto.ControlFromHandle(hwnd)
    ed, rect, btns = inject.scan(use_cache=False)
    print("输入框:", rect, "值:", repr(inject.read_value(ed)[:60]) if ed else None)
    print("底部按钮:", [(c.Name or "", r.left) for c, r in btns])
    print("busy:", inject.is_busy(btns))
    print()

    # 输入框上方区域里的文本控件 = 队列条目
    if not rect:
        print("没有输入框，结束")
        return
    top = rect.top - 320
    rows = []
    for c, d in inject._walk(win, max_depth=14):
        try:
            r = c.BoundingRectangle
            if r.width() <= 0 or r.height() <= 0:
                continue
            if not (top <= r.top < rect.top):
                continue
            nm = (c.Name or "").strip()
            if not nm:
                continue
            rows.append((r.top, r.left, c.ControlTypeName, nm[:70]))
        except Exception:
            continue
    rows.sort()
    seen = set()
    print("输入框上方控件：")
    for t, l, ct, nm in rows:
        k = (t, nm)
        if k in seen:
            continue
        seen.add(k)
        print(f"  y={t:4d} x={l:4d} {ct:16s} {nm!r}")


main()
