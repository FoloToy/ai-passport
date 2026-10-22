from __future__ import annotations

import json
import os
import subprocess
import time

NODE = r'C:\Users\46755\.workbuddy\binaries\node\versions\22.22.2-3\node.exe'
HK = r'C:\Users\46755\WorkBuddy\ai-passport\host\hook.js'
PY = r'C:\Users\46755\WorkBuddy\ai-passport\host\.venv\Scripts\python.exe'
HKPY = r'C:\Users\46755\WorkBuddy\ai-passport\host\hook.py'

WORST = 0.0


def fire(label, args, payload, show=False):
    global WORST
    ts = []
    for _ in range(3):
        t0 = time.time()
        r = subprocess.run(args, input=json.dumps(payload, ensure_ascii=False),
                           capture_output=True, text=True,
                           encoding='utf-8', errors='replace', timeout=60)
        ts.append((time.time() - t0) * 1000)
    best = min(ts)
    WORST = max(WORST, best)
    extra = ''
    if show:
        extra = f'  out={r.stdout.strip()[:60]!r} rc={r.returncode}'
    print(f'{label:28s} {best:6.0f}ms{extra}')


print('== Node 版 ==')
fire('SessionStart', [NODE, HK, 'SessionStart'], {'source': 'startup'})
fire('UserPromptSubmit', [NODE, HK, 'UserPromptSubmit'], {'prompt': '把上周的质检数据整理成表格'})
fire('Notification', [NODE, HK, 'Notification'],
     {'notification_type': 'permission_prompt', 'message': 'needs permission to run Bash'})
fire('Stop', [NODE, HK, 'Stop'], {})
fire('PreToolUse(关)', [NODE, HK, 'PreToolUse'],
     {'tool_name': 'Bash', 'tool_input': {'command': 'ls'}})

print('== Python 版（对照）==')
fire('Stop', [PY, HKPY, 'Stop'], {})

print(f'\n最慢 {WORST:.0f}ms')
