"""直接调用 idf.py，跳过 MSYSTEM 检查（Git Bash 兼容）"""
import os, sys

# 移除 MSYSTEM，让 idf.py 走 main() 路径（必须在 import 之前！）
os.environ.pop('MSYSTEM', None)
os.environ['IDF_PATH'] = 'D:/Espressif/esp-idf-v5.5.3'
os.environ['IDF_TOOLS_PATH'] = 'D:/Espressif'
os.environ['IDF_PYTHON_ENV_PATH'] = 'D:/Espressif/python_env/idf5.5_py3.13_env'
os.environ['ESP_ROM_ELF_DIR'] = 'D:/Espressif/esp-idf-v5.5.3/components/esp_rom'
# 将 ESP-IDF tools 加入 PATH（Git Bash MSYSTEM 下 PATH 不继承）
tools = [
    'D:/Espressif/tools/cmake/3.30.2/bin',
    'D:/Espressif/tools/ninja/1.12.1',
    'D:/Espressif/esp-idf-v5.5.3/components/esptool_py/esptool',
    'D:/Espressif/tools/idf-exe/1.0',
    'D:/Espressif/python_env/idf5.5_py3.13_env/Scripts',
    # ESP32-C3 交叉编译器
    'D:/Espressif/tools/riscv32-esp-elf/esp-14.2.0_20251107/riscv32-esp-elf/bin',
    # CMake
    'D:/Espressif/.espressif/tools/cmake/3.30.2/bin',
]
os.environ['PATH'] = ';'.join(tools) + ';' + os.environ.get('PATH', '')
os.environ.pop('MSYSTEM', None)

# 直接 import idf 模块并调用 main，避免 shell 复写 MSYSTEM
sys.path.insert(0, 'D:/Espressif/esp-idf-v5.5.3/tools')
import idf

idf.main()
