"""把当前分支推到 GitHub fork，并把每一步的失败原因说清楚。

## 为什么需要它

`git push` 失败时只会甩一句 `Permission denied (publickey)`，
看不出到底是「没配 key」「key 没加到账号」还是「仓库不对」。
这里按顺序把三个前置条件各自验一遍，谁不满足直接指出来。

## 两条认证路径

1. **SSH**（推荐，配对一次长期有效）：需要把 `~/.ssh/id_ed25519.pub`
   贴到 https://github.com/settings/ssh/new
2. **HTTPS + Token**：`set GITHUB_TOKEN=ghp_xxx` 后运行本脚本

用法：
    python tools/push_to_github.py            # 自动选可用方式
    python tools/push_to_github.py --check    # 只体检，不推
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
import urllib.request

REPO_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OWNER_REPO = "vito111111/ai-passport"
BRANCH = "feature/workbuddy-companion"


def clean_env() -> dict:
    env = os.environ.copy()
    for k in ("NODE_OPTIONS", "CODEBUDDY_SAFE_DELETE_SANDBOX", "MSYSTEM",
              "GIT_TERMINAL_PROMPT"):
        env.pop(k, None)
    env["GIT_TERMINAL_PROMPT"] = "0"
    return env


def run(args: list[str], timeout: int = 60, env: dict | None = None):
    """跑一条命令，输出按 utf-8→gbk 兜底解码（本机 git 输出是 GBK）。"""
    try:
        r = subprocess.run(args, cwd=REPO_DIR, capture_output=True,
                           timeout=timeout, env=env or clean_env())
    except subprocess.TimeoutExpired:
        return -1, "TIMEOUT"
    raw = (r.stdout or b"") + (r.stderr or b"")
    for enc in ("utf-8", "gbk"):
        try:
            return r.returncode, raw.decode(enc)
        except UnicodeDecodeError:
            continue
    return r.returncode, raw.decode("utf-8", "replace")


def api_post(url: str, token: str, payload: dict) -> tuple[int, dict]:
    req = urllib.request.Request(
        url,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Authorization": f"token {token}",
            "Accept": "application/vnd.github+json",
            "User-Agent": "wb-push",
            "Content-Type": "application/json",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(req, timeout=25) as r:
            return r.status, json.loads(r.read().decode("utf-8"))
    except urllib.error.HTTPError as e:
        try:
            return e.code, json.loads(e.read().decode("utf-8"))
        except Exception:
            return e.code, {}
    except Exception as e:
        return -1, {"error": str(e)}


def api_get(url: str) -> tuple[int, dict]:
    req = urllib.request.Request(url, headers={"User-Agent": "wb-push"})
    try:
        with urllib.request.urlopen(req, timeout=25) as r:
            return r.status, json.loads(r.read().decode("utf-8"))
    except Exception as e:
        return -1, {"error": str(e)}


def check() -> dict:
    """三项体检。"""
    out = {}

    rc, txt = run(["git", "rev-parse", "--abbrev-ref", "HEAD"])
    out["branch"] = txt.strip() if rc == 0 else "?"

    rc, txt = run(["git", "rev-parse", "HEAD"])
    out["head"] = txt.strip()[:7] if rc == 0 else "?"

    # 1) SSH 认证
    rc, txt = run(["ssh", "-o", "BatchMode=yes", "-o", "ConnectTimeout=15",
                   "-T", "git@github.com"], timeout=45)
    out["ssh_ok"] = "successfully authenticated" in txt
    out["ssh_msg"] = " / ".join(l for l in txt.strip().splitlines() if l.strip())[:150]

    # 2) 账号上登记的公钥数（公开信息，无需认证）
    st, js = api_get(f"https://api.github.com/users/{OWNER_REPO.split('/')[0]}/keys")
    out["keys_count"] = len(js) if isinstance(js, list) else f"n/a({st})"

    # 3) 仓库是否存在 + 是否 fork
    st, js = api_get(f"https://api.github.com/repos/{OWNER_REPO}")
    out["repo_ok"] = st == 200
    out["repo_msg"] = (f"private={js.get('private')} default={js.get('default_branch')} "
                       f"fork_of={js.get('parent', {}).get('full_name')}") if st == 200 else f"HTTP {st}"

    out["token"] = bool(os.environ.get("GITHUB_TOKEN"))
    return out


def push_ssh() -> int:
    print("→ 走 SSH 推送…")
    rc, txt = run(["git", "push", "github", BRANCH], timeout=300)
    print(txt.strip()[-1200:])
    return rc


def push_token(token: str) -> int:
    print("→ 走 HTTPS + Token 推送…")
    url = f"https://x-access-token:{token}@github.com/{OWNER_REPO}.git"
    rc, txt = run(["git", "push", url, f"{BRANCH}:{BRANCH}"], timeout=300)
    # 输出里可能带 token，打码
    safe = txt.replace(token, "***")
    print(safe.strip()[-1200:])
    if rc == 0:
        print()
        print("→ 设置 upstream 为 github（不含 token），后续可直接 git push github")
        run(["git", "remote", "set-url", "github", f"git@github.com:{OWNER_REPO}.git"])
    return rc


def verify_remote(token: str = "") -> bool:
    """从远端确认分支确实收到了，不信 push 的输出。"""
    if token:
        st, js = api_post(f"https://api.github.com/repos/{OWNER_REPO}/branches/{BRANCH}",
                          token, {})
        # POST 不适用，改走常规 GET（token 只在 http 头，这里用带 header 的 GET）
        req = urllib.request.Request(
            f"https://api.github.com/repos/{OWNER_REPO}/branches/{BRANCH}",
            headers={"Authorization": f"token {token}",
                     "User-Agent": "wb-push",
                     "Accept": "application/vnd.github+json"},
        )
        try:
            with urllib.request.urlopen(req, timeout=25) as r:
                js = json.loads(r.read().decode("utf-8"))
                st = r.status
        except Exception as e:
            return False
    else:
        st, js = api_get(f"https://api.github.com/repos/{OWNER_REPO}/branches/{BRANCH}")
    if st != 200:
        print(f"  远端分支查询失败 HTTP {st}")
        return False
    sha = (js.get("commit") or {}).get("sha", "")
    print(f"  远端 {BRANCH} = {sha[:7]}")
    rc, local = run(["git", "rev-parse", "HEAD"])
    match = sha.startswith(local.strip())
    print(f"  本地 HEAD      = {local.strip()[:7]}")
    print(f"  → {'一致，推送成功' if match else '不一致！'}")
    return match


def main(argv: list[str]) -> int:
    print("=" * 60)
    print("GitHub 推送体检")
    print("=" * 60)
    info = check()
    for k, v in info.items():
        print(f"  {k:12s} {v}")

    print()
    if info["branch"] != BRANCH:
        print(f"⚠ 当前分支是 {info['branch']}，不是 {BRANCH}")

    if "--check" in argv:
        return 0 if info["ssh_ok"] else 1

    token = os.environ.get("GITHUB_TOKEN", "").strip()

    if info.get("ssh_ok"):
        rc = push_ssh()
        if rc == 0:
            verify_remote()
            return 0
        print("SSH 推送失败，若设置了 GITHUB_TOKEN 可改走 HTTPS")

    if token:
        rc = push_token(token)
        if rc == 0:
            verify_remote(token)
            return 0
        return rc

    print()
    print("=" * 60)
    print("两条路都不通。二选一：")
    print("=" * 60)
    print()
    print("A) 加 SSH 公钥（一次配好，长期有效）")
    print("   打开 https://github.com/settings/ssh/new")
    print("   Title 随便填，Key 粘贴下面整行（一行，不能断）：")
    print()
    try:
        pub = open(os.path.expanduser("~/.ssh/id_ed25519.pub"),
                   encoding="utf-8").read().strip()
        print("   " + pub)
    except OSError:
        print("   (读不到 ~/.ssh/id_ed25519.pub)")
    print()
    print("B) 用 Token（不用配 SSH）")
    print("   生成：https://github.com/settings/tokens  （勾 repo）")
    print("   然后：set GITHUB_TOKEN=ghp_xxx  &&  python tools/push_to_github.py")
    print()
    print(f"注：账号上当前登记的公钥数 = {info['keys_count']}")
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
