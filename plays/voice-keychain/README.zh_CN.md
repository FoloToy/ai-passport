<p align="right">
  <strong>简体中文</strong> · <a href="README.md">English</a>
</p>

# 音效钥匙扣 (Voice Keychain)

一款音效钥匙扣，把 AI Passport 变成随身音效播放器。开机就能播几十个角色包、
上百段中文语音——jojo、meme猫、刘华强、哈基米、奶龙、小明剑魔……随手一按就出声音。

## 发布信息

- **标题**：音效钥匙扣（Voice Keychain）
- **描述**：一款音效钥匙扣，把 AI Passport 变成随身音效播放器；按 OK 即可播放几十个
  角色包里的上百段中文语音。
- **封面**：`voice-keychain-cover.png`（PNG，1024×1024）。

## 功能

- **角色目录**：以可滚动列表浏览全部角色包，每个角色包是一组语音（如 jojo、MC、
  meme猫、刘华强、刘海柱、卡丘米雪儿、卢音、印度阿三、吉伊卡哇、哈基米、奶龙、
  宝宝肚肚打雷、小团团、小明剑魔）。
- **片段列表**：进入角色包，按名字浏览其中的语音片段。
- **一键播放**：按 OK 播放选中片段；内置解码播放 16kHz 单声道 Opus 音频。
- **设置菜单**（长按 OK）：显示当前电量百分比与电压，并调节播放音量。

## 交互方式

整机由三个按键驱动。顶栏显示标题，首页右侧显示电量百分比（如 `97%`）。

- **UP / DOWN**：上下选择。
- **OK**：进入目录 / 选中片段 / 播放。
- **OK 长按**：打开设置，或返回。

过长条目会横向滚动以完整显示；选中行以蓝色高亮。

## 源码

- **源码**：<https://github.com/Shinku-Chen/ai-passport/tree/feature/voice-keychain>，
  入口文件 `main/voice_app.c`（片段索引在 `main/voice_index.h`）

Opus 音频存放在 `voicefs` SPIFFS 数据分区（`esp_vfs_spiffs_register`，挂载于
`/voices`，烧录在 `0x210000`），用标准 POSIX `fopen`/`fread` 路径读取；应用
烧录的是合并固件镜像，数据分区需单独烧录。

## 封面

`voice-keychain-cover.png`（PNG，1024×1024），展示设备屏幕显示的角色目录，
周围环绕本应用能播放的各种玩具钥匙扣与按键玩具。
