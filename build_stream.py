"""直接驱动 idf.py build，流式写日志（避开 skill 脚本 600s 超时）。"""
import os
import subprocess
import sys
import time

PROJ = r'C:\Users\46755\WorkBuddy\ai-passport'
IDF_VENV_PY = r'D:\Espressif\python_env\idf5.5_py3.13_env\Scripts\python.exe'
LOG = os.path.join(PROJ, 'build_stream.log')

env = os.environ.copy()
for k in ('NODE_OPTIONS', 'CODEBUDDY_SAFE_DELETE_SANDBOX', 'MSYSTEM'):
    env.pop(k, None)

cmd = [IDF_VENV_PY, os.path.join(PROJ, 'build_wrapper.py'), '-C', PROJ, 'build']
t0 = time.time()
with open(LOG, 'w', encoding='utf-8', errors='replace') as f:
    p = subprocess.Popen(cmd, cwd=PROJ, env=env, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, text=True,
                         encoding='utf-8', errors='replace', bufsize=1)
    for line in p.stdout:
        f.write(line)
        f.flush()
    rc = p.wait()
print(f'RC={rc} elapsed={time.time() - t0:.0f}s log={LOG}')
sys.exit(rc)
