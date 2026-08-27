<p align="right">
  <a href="voice-guide.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Voice Keychain Guide

This document explains how the **Voice Keychain** product uses voice assets: where the source audio lives, how it is compressed and packed, and how to build and flash the firmware plus its data partition.

## Source assets

Raw audio lives in `assets/project/<directory>/*.mp3|ogg|wav`. Each directory becomes a top-level item in the keychain UI; each audio file becomes a selectable clip. The folder and file names are displayed in the UI, so keep them human-readable.

- Source media are the user's own assets; record their source and redistribution permission before committing raw files. The repository prefers to keep only the transcoded results (see below) to stay slim and avoid shipping original binaries.
- Non-BMP characters (for example emoji such as `🐦`) are replaced with readable Chinese by the transcoder, because the embedded CJK subset font has no glyph for them.

## Transcoding pipeline

Run the transcoder after adding or changing source audio. It requires `ffmpeg` with the `libopus` encoder and a Python environment with `numpy` and `miniaudio`:

```bash
pip install numpy miniaudio     # once
python tools/encode_opus.py
```

The script performs, for each clip:

1. Decode mp3/ogg/wav with `miniaudio` and resample to 16 kHz mono.
2. Apply a low-pass filter (voice bandwidth, about 4 kHz) and trim leading/trailing silence.
3. Encode Opus at 8 kbps with `ffmpeg` (libopus), writing a raw Opus packet stream (each packet is a 2-byte little-endian length plus one Opus frame).
4. Write `assets/audio/dirNN/clipMM.opus`, update `assets/audio/voice_index.json`, regenerate `main/voice_index.h` (compile-time path/name/length table the firmware uses), and pack a `voicefs.img` using the ESP-IDF `spiffsgen.py` tool.

The firmware decodes the frames in a dedicated task and writes PCM to the audio output. The older IMA-ADPCM encoder (`encode_voice.py`) remains in the repository as recorded decision history; the shipped path is Opus.

## Storage layout

`partitions.csv` adds a dedicated SPIFFS data partition:

```csv
voicefs, data, spiffs, 0x210000, 0x5F0000,
```

The firmware mounts it via `esp_vfs_spiffs_register` under `/voices` and reads each clip with ordinary `fopen`/`fread`. SPIFFS is an ESP-IDF built-in component, so no external Managed Component is required. The `voicefs.img` is the content of that partition; it is small relative to the ~5.94 MB (0x5F0000) partition (the coded clips are a few hundred KB).

## Building and flashing

Build the application firmware as usual. The data partition is flashed separately from the merged app image:

```bash
# app + bootloader + partition table
idf.py -p <PORT> flash
# data partition (voice assets) — flash the voicefs.img to the voicefs offset
python -m esptool --chip esp32c3 -p <PORT> write_flash 0x210000 assets/audio/voicefs.img
```

On first boot the partition is mounted with `format_if_mount_failed = true`, so a blank data partition is formatted automatically before reading clips.

## Updating assets

When you add, rename, or remove source audio:

1. Put the files in `assets/project/<directory>/`.
2. Run `python tools/encode_opus.py` to regenerate the clips, `voice_index.h`, and `voicefs.img`.
3. Rebuild the firmware (the `voice_index.h` table changed) and re-flash the data partition.
