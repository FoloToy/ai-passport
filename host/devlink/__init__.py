"""devlink —— AI PASSPORT × WorkBuddy 硬件终端

分层（自下而上，每层只依赖下一层）：

    proto.py    设备↔PC 的消息约定（纯函数，可单测）
    codec.py    ADPCM 编解码 / 重采样 / 语音前处理（纯函数，可单测）
    link.py     串口传输：读写线程、断线重连、音频累积
    speech.py   离线识别（SAPI 子进程）与语音合成（8k PCM）
    inject.py   把文字送进 WorkBuddy 输入框（UIA 写值 + 点发送键）
    terminal.py 终端语义：状态/通知/审批/说话→识别→注入
    server.py   单实例服务：Terminal + 本机 TCP 控制口
    hook.py     WorkBuddy hook 入口（极简转发）
    mcp_server.py  MCP stdio 客户端（转发到控制口）
    cli.py      人用的命令行与自检

设备被当作**纯终端**：它只负责显示被要求显示的东西、上报被按下的键。
所有判断都在 PC 侧，这样换固件不用改逻辑，改逻辑不用动固件。
"""

__version__ = "2.0.0"
