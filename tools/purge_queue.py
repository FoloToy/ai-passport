"""删除输入框上方待发队列里的所有残留条目。

这些是我早先注入的测试文本，卡在 WorkBuddy 的待发队列里，
每轮结束空闲时自动排出一条发出去 —— 就是"同一条消息反复出现"的真因。

每项右侧三个按钮：转发 / 编辑(铅笔) / 删除(垃圾桶)，都在 x=1048 那列。
删除后队列上移，所以每次都点"最上面那一条"的删除键，循环到队列清空。
"""
import sys
import time

sys.path.insert(0, r"C:\Users\46755\WorkBuddy\ai-passport\host")

from devlink import inject  # noqa: E402

QUEUE_X_MIN = 900
QUEUE_X_MAX = 1100


def queue_items(win):
    """返回队列条目：[(text, top, delete_btn), ...]，按 y 从上到下。"""
    items = []
    texts = []
    btns = []
    for ctrl, _d in inject._walk(win, max_depth=26, budget=30000):
        try:
            ct = ctrl.ControlTypeName
            r = ctrl.BoundingRectangle
            if r.width() <= 0 or r.height() <= 0:
                continue
            if ct == "TextControl":
                nm = (ctrl.Name or "").strip()
                # 队列文本在输入框上方、x≈400 起
                if nm and 380 <= r.left <= 900 and r.top < 830:
                    texts.append((r.top, r.left, nm))
            elif ct == "ButtonControl":
                if QUEUE_X_MIN <= r.left <= QUEUE_X_MAX and 560 <= r.top <= 830:
                    btns.append((r.top, r.left, ctrl))
        except Exception:
            continue

    texts.sort()
    btns.sort()
    for t_top, t_left, nm in texts:
        # 同一行的删除键：最右边那个（x 最大），y 最近
        cands = [(abs(b[0] - t_top), -b[1], b[2]) for b in btns if abs(b[0] - t_top) <= 20]
        if not cands:
            continue
        cands.sort()
        items.append((nm, t_top, cands[0][2]))
    return items


def main():
    inject.ensure_ready()
    import uiautomation as auto

    win = auto.ControlFromHandle(inject.find_window())

    for round_i in range(10):
        items = queue_items(win)
        print(f"--- 第 {round_i + 1} 轮：队列 {len(items)} 条 ---", flush=True)
        for nm, top, _b in items:
            print(f"    y={top} {nm[:45]!r}", flush=True)
        if not items:
            print("队列已空", flush=True)
            break

        nm, top, btn = items[0]
        print(f"  删除: {nm[:40]!r}", flush=True)
        try:
            btn.GetInvokePattern().Invoke()
        except Exception as e:
            print(f"    invoke 失败: {e}", flush=True)
            break
        time.sleep(0.8)

    print("\n=== 最终状态 ===", flush=True)
    items = queue_items(win)
    print(f"剩余条目: {len(items)}", flush=True)
    for nm, top, _b in items:
        print(f"  {nm[:50]!r}", flush=True)


if __name__ == "__main__":
    main()
