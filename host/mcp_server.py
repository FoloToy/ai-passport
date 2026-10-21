"""入口：MCP stdio 服务。

    python mcp_server.py        # 由 ~/.workbuddy/mcp.json 拉起

本进程不碰串口，只把工具调用转给后台服务。
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from devlink.mcp_server import main   # noqa: E402

if __name__ == "__main__":
    main()
