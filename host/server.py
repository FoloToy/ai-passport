"""入口：后台服务。

    python server.py

常驻进程，独占串口，对外开 127.0.0.1:37821。
hooks / MCP / 命令行都通过这个口子说话，所以它必须在跑。
"""
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from devlink.server import main   # noqa: E402

if __name__ == "__main__":
    sys.exit(main())
