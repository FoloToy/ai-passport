<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Voice Keychain

A sound-effects keychain that turns the AI Passport into a pocket audio player.
Open it and instantly play one of hundreds of Chinese voice clips from dozens of
character packs — jojo, meme cat, Liu Huaqiang, Haji Mi, Nailong, Xiao Ming
Jian Mo, and more.

## Publish information

- **Title**: Voice Keychain
- **Description**: a sound-effects keychain that turns the AI Passport into a
  pocket audio player; press OK to play one of hundreds of Chinese voice clips
  from dozens of character packs.
- **Cover**: `voice-keychain-cover.png` (PNG, 1024×1024).

## What it does

- **Character directory**: browse all character packs as a scrollable list. Each
  entry is a pack of voice clips (e.g. jojo, MC, meme cat, Liu Huaqiang,
  Liu Haizhu, Kaqiu Mixue'er, Luyin, Indian A-san, Ji Yi Kawai, Haji Mi,
  Nailong, Bao Bao Duda Leilei, Xiao TuanTuan, Xiao Ming Jian Mo).
- **Clip list**: enter a pack to see its clips by name.
- **One-tap playback**: press OK to play the selected clip; built-in decoding
  plays 16 kHz mono Opus audio.
- **Settings** (hold OK): show current battery percentage and voltage, and
  adjust the playback volume.

## Interaction

Three keys drive the whole app. A top bar shows the title and, on the home
screen, the battery percentage (e.g. `97%`).

- **UP / DOWN**: move selection.
- **OK**: enter a directory / select a clip / play.
- **OK (hold)**: open settings, or go back.

Long entries scroll horizontally so the full name is readable; the selected row
is highlighted in blue.

## Source

- **Source**: <https://github.com/Shinku-Chen/ai-passport/tree/feature/voice-keychain>,
  entry file `main/voice_app.c` (clip index in `main/voice_index.h`)

The Opus audio clips live in a `voicefs` SPIFFS data partition
(`esp_vfs_spiffs_register`, mounted at `/voices`, flashed at `0x210000`), read
with the standard POSIX `fopen`/`fread` path; the app flashes the merged firmware
image and that data partition separately.

## Cover

`voice-keychain-cover.png` (PNG, 1024×1024) shows the device screen displaying
the character directory, surrounded by the toy keychains and button toys that
the app plays.
