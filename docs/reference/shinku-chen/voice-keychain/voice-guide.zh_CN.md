<p align="right">
  <strong>简体中文</strong> · <a href="voice-guide.md">English</a>
</p>

# 音效钥匙扣指南

本文说明**音效钥匙扣**产品如何使用语音素材：原始音频放在哪、如何压缩打包、以及如何构建并烧录固件与其数据分区。

## 原始素材

原始音频位于 `assets/project/<目录>/*.mp3|ogg|wav`。每个目录对应钥匙扣 UI 里的一个顶层项目；每个音频文件对应一段可选择的音效。目录名与文件名会显示在 UI 中，请保持其可读。

- 原始媒体是用户自有素材；提交原始文件前应记录来源与再分发许可。本仓库倾向只保留转码产物（见下文），以保持精简并避免打包原始二进制。
- 文件名中的非 BMP 字符（如 emoji `🐦`）会被转码器替换成可读中文，因为嵌入的 CJK 子集字体没有对应字形。

## 转码管线

新增或改动原始音频后运行转码器，需一个带 `libopus` 编码器的 `ffmpeg`、以及装有 `numpy` 与 `miniaudio` 的 Python 环境：

```bash
pip install numpy miniaudio     # 首次
python tools/encode_opus.py
```

脚本对每段执行：

1. 用 `miniaudio` 解码 mp3/ogg/wav 并重采样到 16 kHz 单声道。
2. 低通滤波（语音带宽约 4 kHz）并剪除首尾静音。
3. 用 `ffmpeg`（libopus）编码为 Opus 8 kbps，写成裸 Opus 包流（每个包 = 2 字节小端长度 + 一个 Opus 帧）。
4. 写入 `assets/audio/dirNN/clipMM.opus`，更新 `assets/audio/voice_index.json`，重新生成 `main/voice_index.h`（固件使用的编译期路径/中文名/长度表），并用 ESP-IDF 的 `spiffsgen.py` 打包 `voicefs.img`。

固件在独立任务中逐帧解码并写出 PCM 到音频输出。旧的 IMA-ADPCM 编码器（`encode_voice.py`）保留在仓库作为已记录的决策历史；实际交付路径是 Opus。

## 存储布局

`partitions.csv` 新增一个独立的 SPIFFS 数据分区：

```csv
voicefs, data, spiffs, 0x210000, 0x5F0000,
```

固件通过 `esp_vfs_spiffs_register` 将其挂载到 `/voices`，并用普通的 `fopen`/`fread` 读取每段。SPIFFS 是 ESP-IDF 内置组件，因此无需外部 Managed Component。`voicefs.img` 即该分区的内容；相对约 5.94 MB（0x5F0000）分区它很小（编码后的音效仅数百 KB）。

## 构建与烧录

按常规构建应用固件。数据分区与合并的应用镜像分开烧录：

```bash
# 应用 + bootloader + 分区表
idf.py -p <端口> flash
# 数据分区（语音素材）— 把 voicefs.img 烧到 voicefs 偏移
python -m esptool --chip esp32c3 -p <端口> write_flash 0x210000 assets/audio/voicefs.img
```

首次启动时以 `format_if_mount_failed = true` 挂载，因此空白数据分区会在读取前自动格式化。

## 更新素材

当新增、重命名或删除原始音频时：

1. 把文件放入 `assets/project/<目录>/`。
2. 运行 `python tools/encode_opus.py` 重新生成各段、`voice_index.h` 与 `voicefs.img`。
3. 重新构建固件（`voice_index.h` 表已改变）并重新烧录数据分区。
