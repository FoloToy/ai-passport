# Windows Hardware Buddy Controller

这是一个 Tkinter 图形测试工具，通过 Windows BLE 模拟 Claude Desktop 的 Hardware
Buddy bridge。它使用官方 Nordic UART Service 和换行分隔 JSON，可手动或自动发送
heartbeat、time、owner、name、status、unpair 和审批 prompt，并显示设备返回的审批
决定与状态响应。

## 环境

- Windows 11
- Python 3.11 或更高版本（安装时勾选 **Add Python to PATH**）
- 电脑具备 Bluetooth LE

## 安装和启动

在 PowerShell 中进入仓库：

```powershell
cd C:\path\to\trae_card_bsp
py -3.11 -m venv .venv-buddy
.\.venv-buddy\Scripts\Activate.ps1
python -m pip install -r tools\windows_buddy_controller\requirements.txt
python -m tools.windows_buddy_controller.app
```

如果只把 `windows_buddy_controller` 文件夹复制到 Windows 桌面，也可以直接进入该
目录启动：

```powershell
python -m pip install -r requirements.txt
python .\app.py
```

若 PowerShell 禁止激活脚本，可直接运行：

```powershell
.\.venv-buddy\Scripts\python.exe -m pip install -r tools\windows_buddy_controller\requirements.txt
.\.venv-buddy\Scripts\python.exe -m tools.windows_buddy_controller.app
```

## 使用

1. 给 FoloToy-Card 烧录 Claude Buddy 固件并开机。
2. 点击 **Scan**，选择 `Claude-*` 设备并点击 **Connect**。
3. 卡片显示六位密码后，在 Windows 系统配对提示中输入该密码。
4. 连接后工具自动发送本地时间、Owner 和首个 heartbeat，此后每 10 秒保活。
5. 使用 Idle、Busy、Completed 或 Approval 按钮验证页面和审批交互。
6. 在卡片上批准或拒绝，工具会在日志中收到 `permission` JSON。
7. **Automatic cycle** 会依次运行 Idle、Busy、Approval、Completed 和 Sleep 场景。

注意：Sleep snapshot 仍会发送 heartbeat，只是把会话数设为零。若要验证固件的 30 秒
心跳超时，请点击 **Disconnect**，或退出本工具。

## 故障排查

- 扫描不到设备：确认设备名以 `Claude` 开头、BLE 已开启，并在 Windows 设置中打开
  蓝牙。关闭正在占用设备的 Claude Desktop 或其他 BLE 调试工具。
- 没有配对窗口：在 Windows **设置 → 蓝牙和设备** 中删除旧的设备绑定，然后在卡片
  Settings 中执行 Unpair，再重新连接。
- 已连接但无法发送：确保 Windows 已完成六位密码配对；固件要求加密、认证、绑定和
  TX notification 订阅。
- `Access denied`：关闭其他连接该设备的应用，然后重新启动控制器。

## 测试

协议和自动场景测试不需要 BLE 硬件：

```powershell
$env:PYTHONPATH = "tools"
python -m unittest discover -s tools\windows_buddy_controller\tests -v
```
