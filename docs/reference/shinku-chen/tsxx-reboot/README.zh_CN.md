<p align="right">
  <a href="README.md">English</a> · <strong>简体中文</strong>
</p>

# 天使☆騒々 RE-BOOT! 阅读器

AI Passport 上的 240×320 竖屏视觉小说阅读器。固件内置完整的中文译文、六条女主线
与十五个结局，全程离线，不依赖网络或外部文件。

## 规模

- 61,436 页剧本、61,289 句对白、1,165,911 字正文，共 45 个章节点。
- 13 个选择点、15 个结局点：6 条女主线（Noa / Amane / Kurumi / Kaguya / Orie /
  Fumika）、2 个 BAD END、1 个 END、6 个回标题页。
- 6.26 MiB 资源包；16 px 中文字体子集包含 3,417 个码位。

## 操作

| 按键 | 阅读中 | 列表 / 选项 |
| --- | --- | --- |
| 短按 OK | 推进一句；打字中按下先显示全文 | 确认 |
| 长按 OK | 打开菜单 | 返回上一层 |
| 短按 UP / DOWN | 推进一句 | 移动光标 |
| 长按 UP | 快进，松手停 | - |
| 长按 DOWN | 开关自动阅读 | - |

标题页用 UP / DOWN 选择，OK 进入，长按 OK 关机。菜单提供继续阅读、保存、读取、
跳过章节与章节跳转。

## 实现说明

- 阅读器沿用 ATRI / 星空列车的 LVGL 页面系统，并补上原作的数据驱动分支：15 个结局、
  5 个 flag 闸门（11 条规则 / 54 个条件）、18 个禁进与 38 个禁退跳转，全部来自
  `branch.json`；闸门指向不存在的选项时构建直接失败。
- 150 KB RGB565 画布不配解码暂存区：背景、事件图、补丁与立绘通过 tjpgd 回调直接
  解码进画布；立绘按 1bpp 遮罩逐行从 Flash blit。
- 约 1.9 MiB 的脚本区间（页表、正文码流、选项表、分支表）在运行期常驻只读映射；
  图片段使用 256 KB 滑动 `esp_partition_mmap` 窗口，取图即解码。
- host 测试逐页走完整张页表，并单独走完六条女主线，覆盖结局点、闸门与分页排版。

## 发布

- 分支：`feature/tsxx-reboot`
  （<https://github.com/Shinku-Chen/ai-passport/tree/feature/tsxx-reboot>）
- Release：`v0.1.1-tsxx-reboot`
  （<https://github.com/Shinku-Chen/ai-passport/releases/tag/v0.1.1-tsxx-reboot>）
- 社区条目：<https://ai-passport.folotoy.cn/plays/re-boot-galgame>
- 固件：`FoloToy-AI-Passport-full.bin`，**8,135,520 B**（合并镜像）

## 上游

- 来源：[`hezdaaa/tsxxreboot-miband`](https://github.com/hezdaaa/tsxxreboot-miband)
  —— 小米手环快应用同人移植，未声明许可；剧本、立绘与背景著作权归柚子社及原发行方。
- 阅读器基线：本 fork 的 ATRI / 星空列车 LVGL 页面系统。
