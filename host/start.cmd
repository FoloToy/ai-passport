@echo off
rem 手动启动后台服务（AI PASSPORT × WorkBuddy）
rem 双击即可。已在跑时会提示"已有实例"并退出，不会冲突。
chcp 65001 >nul
cd /d "%~dp0"
".venv\Scripts\python.exe" server.py
pause
