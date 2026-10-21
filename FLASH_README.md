# FoloToy AI-Passport 固件烧录说明

## 当前固件状态

| 产物 | 路径 | 大小 |
|------|------|------|
| App 固件 | `build/FoloToy-AI-Passport.bin` | 1,551,616 bytes |
| 完整合并固件 | `build/FoloToy-AI-Passport-full.bin` | 1,617,152 bytes |
| ELF（含调试符号） | `build/FoloToy-AI-Passport.elf` | 16,278,448 bytes |
| Bootloader | `build/bootloader/bootloader.bin` | 21,024 bytes |
| 分区表 | `build/partition_table/partition-table.bin` | 3,072 bytes |

固件 hash: `a559e13cccd97c8159fa05062bce93d0f5c8b6f258504863a41a59c15b4672c9`

---

## 烧录方式

### 方式一：一键脚本（推荐）

1. 确保 ESP32-C3 已通过 USB 连接电脑
2. 确认设备管理器中显示为 COM 端口（如 COM3）
3. 双击 `flash_it.bat`，按提示确认
4. 等待烧录完成（约 30 秒）

### 方式二：命令行

```cmd
cd C:\Users\46755\WorkBuddy\ai-passport
idf.py -p COM3 flash
```

### 方式三：手动 esptool

```cmd
esptool --chip esp32c3 -b 460800 --before default_reset --after hard_reset write_flash ^
  --flash_mode dio --flash_size 8MB --flash_freq 80m ^
  0x0 build\bootloader\bootloader.bin ^
  0x8000 build\partition_table\partition-table.bin ^
  0x10000 build\FoloToy-AI-Passport.bin
```

---

## 烧录后

查看串口日志：
```cmd
idf.py -p COM3 monitor
```

---

## 如需重新构建固件

```cmd
cd C:\Users\46755\WorkBuddy\ai-passport
python build_firmware2.py
```

构建完成后运行 `python run_validate.py` 验证固件。

---

## 已知限制

- Wi-Fi 已禁用（`CONFIG_ESP_WIFI_ENABLED=n`）
  - 原因：ESP-IDF 5.5.3 ESP32-C3 `esp_wifi` 链路缺少 WPA3 函数
  - BLE（NimBLE 1.6.0）功能完整，玩具对话交互正常
