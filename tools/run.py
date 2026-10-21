"""执行一段子进程，把输出写到同目录文件——绕开损坏的 bash shim。"""
import subprocess
import sys

PY = r"C:\Users\46755\WorkBuddy\ai-passport\host\.venv\Scripts\python.exe"
target = sys.argv[1]
out = sys.argv[2] if len(sys.argv) > 2 else target + ".out"
r = subprocess.run([PY, "-u", target], capture_output=True, text=True,
                   encoding="utf-8", errors="replace", timeout=300)
with open(out, "w", encoding="utf-8") as f:
    f.write("RC=%d\n" % r.returncode)
    f.write("--- STDOUT ---\n" + (r.stdout or "") + "\n")
    f.write("--- STDERR ---\n" + (r.stderr or "")[-1500:] + "\n")
print("written", out)
