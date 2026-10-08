<p align="right">
  <strong>简体中文</strong> · <a href="cjk-line-pitch-from-font-metrics.md">English</a>
</p>

# 行距要按字体度量算,不能按生成字号算

本条目来自 **limelight lemonade jam 阅读器**（`v0.1.0-limelight`）。真机验收时发现五行正文
溢出文本框——最后一行被画到读者自己拥有的版面区域之外，在屏幕上看就是"对话框被文字填满"。
修好后在真机上回读帧、数墨迹行做了验证。

## LVGL 实际用的公式

LVGL 标签的行距是：

```text
pitch = font->line_height + text_line_space
```

`line_height` 属于字体；而对中日韩子集来说，它通常**比生成字号更大**：用 Noto Sans SC 生成的
16px 子集报告 `line_height = 20`、`base_line = 3`。于是把样式写成"生成字号减去字面框"——正是本
fork 早先几个阅读器的写法：

```c
/* 从模板抄来的,对这个字体是错的 */
lv_obj_set_style_text_line_space(ui->body, LIME_UI_LINE_H - 16, 0);
```

实际行距就变成 24px 而不是想要的 20px。五行于是需要 120px，而正文框只有 100px，第五行被画到框外。

## 修法

按标签真正使用的字体算间距，这样换字体也能保持行距等于版面常量：

```c
lv_obj_set_style_text_line_space(
    ui->body, LIME_UI_LINE_H - (int32_t)ui->font_cjk->line_height, 0);
lv_obj_set_style_max_height(ui->body, LIME_UI_LINES * LIME_UI_LINE_H, 0);
```

再加一条编译期护栏，避免这个关系被悄悄改坏——分页用的每页行数与标签几何必须一致：

```c
_Static_assert(LIME_UI_LINES * LIME_UI_LINE_H + 7 <= LIME_UI_BOX_H,
               "正文行数×行高必须放得进正文框(含 7px 上边距)");
```

## 在真机上量,而不是相信排版代码

阅读器可以把整帧回传到串口，所以行距是可以**量**出来的：回读 240 × 214 画面区加正文带，数含
文字墨迹的行即可。本机实测记录：

| | 墨迹行（帧坐标） | 行距 |
| --- | --- | --- |
| 修复前 | 220–235、244–259 | **24 px** |
| 修复后 | 220–235、240–255、260–275、280–295、300–315 | **20 px** |

第二行就是验收证据：五行 × 20px 正好占 100px，最后一行结束在 315 行，没有任何内容画到正文带
（覆盖 214–319 行）之外。

## 这个写法从哪来

硬编码的 `- 16` 是从本 fork 早先一个视觉小说阅读器抄来的，它的字体恰好同一种生成方式，而对白
又短到很少触发溢出。**只要涉及中日韩子集，就把"生成字号"和"字体行高"当两个不同的数**，并且
尽量用一条静态断言把分页版面与标签盒子绑在一起。
