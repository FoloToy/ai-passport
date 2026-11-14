<p align="right">
  <strong>简体中文</strong> · <a href="lvgl-chinese-fonts.md">English</a>
</p>

# LVGL 中文显示：接入与排查

添加中文文案、修改字体/字号或显示用户/网络内容时使用本指南。[代码检查清单](coding-conventions.zh_CN.md#中文字体与缺字排查)定义验收要求，本文提供当前仓库锁定的 LVGL 9.5.0 的具体接入步骤。以下都是应用开发示例，不代表基线已经启用了这些配置。

## 1. 区分编码、字形覆盖和样式选择

正确显示需要同时满足以下条件：

| 层次 | 必须满足 | 常见错误表现 |
| --- | --- | --- |
| 文本输入 | 有效 UTF-8，不能截断多字节字符 | 即使字体正确也显示乱码 |
| 字体素材 | 每个所需 Unicode 码点都在字体或 fallback 链中 | 英文正常，中文空白/方框 |
| 编译接入 | 字体源文件已编译、链接并启用 | 字体符号未定义，或预期字体根本没用上 |
| 控件样式 | 当前状态下，实际绘制文字的部件选用了该字体 | 只有标题、弹窗或选中项异常 |
| 布局/绘制 | 颜色/透明度可见、空间足够，字体生命周期和绘制内存有效 | 字形存在，但文字被裁剪或不可见 |

默认配置中的 Montserrat 14/20 不含中文字形。UTF-8 只规定怎样解码文本，不会安装字库。此版本 LVGL 内置的 Source Han Sans SC 也只是子集，不是完整中文字库。必须同时核对字体素材和控件样式；参见 [LVGL 字体概览](https://docs.lvgl.io/9.5/main-modules/fonts/overview.html)。

## 2. 核对应用实际使用的配置

在 `idf.py menuconfig` 中搜索以下符号，并检查生成的 `sdkconfig`：

```text
CONFIG_LV_TXT_ENC_UTF8=y
CONFIG_LV_USE_FONT_PLACEHOLDER=y
```

开发时保留缺字占位，让问题可见。关闭占位可能把缺字变成空白，并没有补齐字形。不要修改生成的头文件或 `managed_components/` 内的文件来强行覆盖这些配置。

源码和文本资源保存为 UTF-8。JSON 转义应先解码再显示，旧编码输入应在应用边界转换。不要按任意字节数截断 UTF-8；既要给结尾 NUL 留空间，也要确保截断位置在完整字符边界。`lv_label_set_text()` 会复制字符串，`lv_label_set_text_static()` 则要求传入缓冲持续有效，不能让它在栈局部缓冲离开作用域后继续使用该地址。

本地构建检查生成的根目录 `sdkconfig`，不能只看 `sdkconfig.defaults`。仓库固件门禁会从已跟踪的 defaults 生成隔离配置。把应用有意使用的设置写入该应用已跟踪的 defaults，并按[构建与验证](build-and-test.zh_CN.md)核对两种配置。

## 3. 方案 A：用内置 CJK 子集快速验证

小范围初步实验可以在应用配置中启用：

```text
CONFIG_LV_FONT_SOURCE_HAN_SANS_SC_16_CJK=y
```

重新编译，然后在标签上显式指定字体。以下函数只在 LVGL/显示初始化后调用，必须处于 LVGL 任务或持有 `bsp_lvgl_lock()`；传入可见且有足够空间的父对象：

```c
#include "lvgl.h"

lv_obj_t *app_create_cjk_probe(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &lv_font_source_han_sans_sc_16_cjk,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "\u4E2D\u6587 ABC 123");
    return label;
}
```

转义表示 U+4E2D、U+6587，即“中文”，用来保持中英文文档示例一致；合法 UTF-8 源码也可直接写中文。这个探针只测试少量字符。用于产品前，必须检查实际文案、标点、姓名和支持的输入。启用字体不等于全局选用它；设置主题/默认字体也不会替换现有控件显式指定的字体。

## 4. 方案 B：生成可复现的应用子集

准备授权允许且覆盖所需字形的 TTF/OTF 字体。可复用素材放在 `assets/fonts/`，来源/授权记录在[素材 README](../../../assets/README.zh_CN.md)。安装固定版本的[官方字体转换工具](https://github.com/lvgl/lv_font_conv)，记录版本，并通过 `lv_font_conv --help` 核对参数。

先创建素材目录，把输入路径换成实际字体，再从仓库根目录运行：

```bash
lv_font_conv \
  --font /path/to/licensed-cjk-font.otf \
  --range 0x20-0x7E,0x4E2D,0x6587,0xFF0C,0x3002 \
  --size 20 --bpp 4 --format lvgl --no-compress \
  --lv-font-name app_font_20 --lv-include lvgl.h \
  --output assets/fonts/app_font_20.c
```

示例只请求可打印 ASCII、“中文”、全角逗号和中文句号，不是正式应用的字符清单。请把范围替换为全部所需码点，或使用转换器的 `--symbols` 输入实际文案。源字体没有的字形，不能靠填写码点生成；必须检查转换提示及生成结果的覆盖范围，不能把命令成功等同于字形齐全。

将字符清单和转换命令纳入版本管理。每个实际字号分别生成，使用 `app_font_16`、`app_font_20` 等不同名称。首次接入先不压缩，避免解码器/配置不匹配；后续若使用压缩字体，启用 `CONFIG_LV_USE_FONT_COMPRESSED=y` 并测量绘制与内存影响。不要为了补一个字就启用所有字号或整套 CJK 字体家族。

### 编译并声明生成的源文件

对于当前仓库的 `main` 组件，在 `main/CMakeLists.txt` 现有 `idf_component_register(...)` 之后追加以下内容，不要替换原有源文件或依赖列表：

```cmake
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_20.c"
)
```

也可以把源文件加入组件已有的 `SRCS` 列表，两种方式选一种即可。不要重复编译同一字体，也不要在其他源码里 `#include` 字体 `.c`。在应用源文件或共享字体头文件中包含 `lvgl.h` 并使用 `LV_FONT_DECLARE(app_font_20)`；声明必须匹配生成的符号，声明本身不会定义或链接字体。若独立建立字体组件，该组件必须声明 LVGL 依赖。

## 5. 绑定文字与图标，不修改只读字体

若 `app_font_20` 包含中文和 ASCII，但没有应用使用的 LVGL 图标，可以 fallback 到默认已经启用的 Montserrat 20。对于这里的静态生成字体，保留一个与应用同寿命的可写描述符，并在任何控件使用它之前初始化一次：

```c
#include "lvgl.h"

LV_FONT_DECLARE(app_font_20);
static lv_font_t s_app_font_with_symbols;

void app_fonts_init(void)
{
    s_app_font_with_symbols = app_font_20;
    s_app_font_with_symbols.fallback = &lv_font_montserrat_20;
}

lv_obj_t *app_create_font_probe(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &s_app_font_with_symbols,
                               LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_label_set_text(label, "\u4E2D\u6587\uFF0C ABC 123\u3002 " LV_SYMBOL_OK);
    return label;
}
```

先完成 LVGL 初始化，再调用 `app_fonts_init()`，最后创建标签。UI 操作仍须在 LVGL 任务或 `bsp_lvgl_lock()` 内执行。此处描述符浅拷贝只针对示例的静态位图字体，不是动态加载字体引擎的通用复制方式。描述符及引用的数据必须覆盖全部使用期；不得强制去掉原字体的 `const`，也不得形成 fallback 环路。

Fallback 有方向：这里先查应用字体，仅在缺字时使用 Montserrat。两边都没有的中文仍然无法显示。标称字号相同不保证基线、行高相同；必须检查混排行并留足高度，必要时重新生成度量兼容的字体。若希望英文优先使用另一种字体，应通过应用持有的描述符明确调整主字体/fallback 方向，并重新核对度量与覆盖。

检查实际控件绑定，不能只看 screen/默认字体。`ui_pixel_label()` 设置局部字体，`ui_pixel_screen_create()` 的标题使用 Montserrat 20，菜单/demo 标签也有显式 Montserrat 配置。新应用应实现自己的 UI/字体辅助函数。复合控件需检查真正绘制文字的部件，以及单独创建的弹出列表；焦点、按下、选中和禁用状态都要检查。

## 6. 验证字形覆盖，查明当前字体

以下辅助函数检查 Unicode 码点，而不是 UTF-8 单字节或 C 的多字符字符常量：

```c
#include "lvgl.h"

bool app_font_has_glyph(const lv_font_t *font, uint32_t codepoint)
{
    if (font == NULL) return false;
    lv_font_glyph_dsc_t glyph = {0};
    return lv_font_get_glyph_dsc(font, &glyph, codepoint, 0)
        && !glyph.is_placeholder;
}
```

对于已有标签，在 LVGL 锁内调用 `lv_obj_get_style_text_font(label, LV_PART_MAIN)`，对获取的实际字体进行检查，而不是验证一个无关的全局字体符号。这个 getter 解析当前状态，各相关状态切换后都要重复检查。返回成功只说明描述符/fallback 字形覆盖，不证明位图解码和屏幕输出成功。

把所有固定文案解码为 Unicode 码点，按各字号/样式真正需要的字符检查；换行等控制字符不纳入可打印字形检查。缺字记录为 `U+XXXX`。对于上述生成子集，若源字体包含所请求字形，U+4E2D/U+6587/U+FF0C/U+3002 应通过；U+9F98 刻意未收录，使用示例中的 Montserrat fallback 时应失败。加入已知缺字的反例，避免覆盖测试无条件通过。

动态内容必须有明确约定：只接受已声明字符集、补齐经过验证的字形，或显示有意设计且能正确显示的替代字符/不支持提示。不要悄悄丢弃未知字符，也不要把固定菜单子集描述成支持任意姓名和网络文本。支持范围变化后，重新生成字体并运行覆盖检查。

### 静态覆盖门禁

`./tools/validate.sh --static` 会运行 [`tools/check_glyphs.py`](../../../tools/check_glyphs.py)。它从 `assets/fonts/` 和 `main/` 下生成字体的 `lv_font_fmt_txt_cmap_t` 表读取字体实际包含的码点（而不是生成时请求的字符），`main/` 中字符串字面量用到字体之外的非 ASCII 字符时失败，并按 `文件:行号: U+XXXX` 列出每个缺字。它会解码 `\uXXXX` 转义，检查 `#define` 内容和 `snprintf` 格式串，跳过注释、`#if 0` 块、`#include` 路径，以及传给 `ESP_LOG*`、`printf`、`puts` 或断言的字面量。永不显示的行可加 `glyph-check: ignore` 注释跳过。没有内置字体的工程（例如本基线）报告 `SKIPPED`。

该门禁检查所有内置字体的并集，无法判断控件实际选用哪种字体，看不到运行时拼接、用户输入或网络下发的文字，也不能证明字形能正确绘制。上文的逐字体检查和第 8 节的真机检查仍然需要。

## 7. 按顺序定位故障

| 现象 | 下一步 |
| --- | --- |
| 字体 undefined reference | 检查符号拼写、CMake 源文件注册、字体启用宏，以及 C++ 使用时是否有兼容 C 的声明 |
| 字体文件存在但界面没变化 | 检查实际选中的字体；复制素材或声明符号都不够 |
| ASCII 正常，中文空白/方框 | 检查编码、逐码点覆盖和实际 fallback 链；不要关闭占位 |
| 正文正常，标题/弹窗异常 | 检查局部字体、控件部件、单独创建的子对象/弹窗及状态覆盖 |
| 字形检查通过但仍空白 | 检查位图格式/解码器、字体生命周期、文字颜色/透明度、隐藏标志和 LVGL 分配错误 |
| 字符重叠或上下被切掉 | 检查行高、基线、父容器裁剪、标签宽高和 fallback 度量 |
| 编译成功但新文案异常 | 重新生成受影响的所有子集/字号，检查实际配置，并确认设备运行的是目标构建 |

## 8. 验证与交付边界

运行[完整仓库门禁](build-and-test.zh_CN.md)和应用自己的字符覆盖测试。用 `idf.py size-components`/`idf.py size-files` 检查 Flash/静态 RAM，同时记录真实 UI 和网络启用时的 free heap、largest free block。只读 Flash 中的字体并不等于把整个字库分配到堆中，但绘制/加载/缓存仍可能需要内部 RAM。

真机逐页逐状态检查混排/图标、长文本、不同字号和支持范围内外的动态输入。页面重建、重启后也要测，不能只看第一个标签。记录固件版本、字体/转换器版本、测试字符清单，必要时留截图，列出任何缺字与排版问题。未完成这些检查时，中文显示仍属于 `Unverified`；基线 host tests 或固件编译通过都不能验证这些示例的实际显示效果。
