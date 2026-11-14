<p align="right">
  <a href="lvgl-chinese-fonts.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# LVGL Chinese Text: Integration and Troubleshooting

Use this guide when adding Chinese text, changing fonts/sizes, or displaying user/network content. The [coding checklist](coding-conventions.md#chinese-fonts-and-missing-glyphs) defines acceptance; this page provides the integration procedure for the LVGL 9.5.0 version pinned in this repository. These are application examples, not changes already enabled in the baseline.

## 1. Keep encoding, coverage, and style selection separate

Correct rendering requires all of the following:

| Layer | Required condition | Typical failure |
| --- | --- | --- |
| Text input | Valid UTF-8, with no truncated multibyte characters | Corrupted text even with the correct font |
| Font asset | Each requested Unicode code point exists in the font or fallback chain | Chinese is blank/boxed while Latin text works |
| Build integration | Font source is compiled, linked, and enabled | Undefined font symbol or the intended font never being used |
| Widget style | The text-drawing part actually selects that font in the active state | Only titles, popups, or selected items fail |
| Layout/rendering | Visible color/opacity, sufficient space, valid font lifetime and draw memory | Clipped or invisible text despite correct coverage |

Montserrat 14/20 in the default configuration has no Chinese glyphs. UTF-8 only describes how to decode text; it does not install a font. Source Han Sans SC built into this LVGL version is also a subset, not the complete Chinese character set. The font asset and widget style must both be checked; see the [LVGL font overview](https://docs.lvgl.io/9.5/main-modules/fonts/overview.html).

## 2. Verify the configuration actually used by the application

In `idf.py menuconfig`, search for the following symbols and check the generated `sdkconfig`:

```text
CONFIG_LV_TXT_ENC_UTF8=y
CONFIG_LV_USE_FONT_PLACEHOLDER=y
```

Keep placeholders enabled while developing so missing glyphs remain visible. Disabling them can turn missing text into blanks; it does not repair coverage. Do not edit generated headers or files under `managed_components/` to force these settings.

Save source files and text resources as UTF-8. Decode JSON escapes before display, and convert any legacy-encoded input at the application boundary. Do not truncate UTF-8 by an arbitrary byte count; leave space for the terminating NUL and cut only at a character boundary. `lv_label_set_text()` copies its string, while `lv_label_set_text_static()` requires the supplied buffer to remain valid; a stack-local buffer must not outlive its scope through the latter API.

For local builds, inspect the generated root `sdkconfig`, not just `sdkconfig.defaults`. The repository's firmware gate instead generates an isolated configuration from tracked defaults. Persist intentional settings in your application's tracked defaults and verify both configurations as described in [build and test](build-and-test.md).

## 3. Option A: validate a built-in CJK subset

For a small initial experiment, enable this font in your application's configuration:

```text
CONFIG_LV_FONT_SOURCE_HAN_SANS_SC_16_CJK=y
```

Rebuild, then explicitly select it on a label. The following function is called only after LVGL/display initialization, in the LVGL task or while holding `bsp_lvgl_lock()`. Supply a visible parent with adequate space:

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

The escapes identify U+4E2D and U+6587 and keep the example identical in both document languages. Valid UTF-8 source literals work too. This probe tests a few characters only. Before choosing this font for a product, check every actual label, punctuation mark, name, and supported input. Enabling the font does not select it globally; setting a theme/default font still does not replace explicit fonts in existing widgets.

## 4. Option B: generate a reproducible application subset

Prepare a licensed TTF/OTF font that contains the required glyphs. Store reusable materials under `assets/fonts/` and record their source/license in the [assets README](../../../assets/README.md). Install a pinned version of the [official converter](https://github.com/lvgl/lv_font_conv), record its version, and confirm the options with `lv_font_conv --help`.

Run from the repository root, after creating the asset directory and replacing the example input path with your font:

```bash
lv_font_conv \
  --font /path/to/licensed-cjk-font.otf \
  --range 0x20-0x7E,0x4E2D,0x6587,0xFF0C,0x3002 \
  --size 20 --bpp 4 --format lvgl --no-compress \
  --lv-font-name app_font_20 --lv-include lvgl.h \
  --output assets/fonts/app_font_20.c
```

This example requests printable ASCII, two Chinese characters, a full-width comma, and an ideographic full stop. It is not a production character inventory. Replace the range with every required code point, or use the converter's `--symbols` input with your real text. An absent glyph in the source font cannot be created by requesting its code point. Check conversion diagnostics and the generated coverage rather than assuming success means completeness.

Keep the character inventory and conversion command under version control. Generate each required size separately, using distinct names such as `app_font_16` and `app_font_20`. Start uncompressed to avoid a decoder/configuration mismatch; if compression is introduced, enable `CONFIG_LV_USE_FONT_COMPRESSED=y` and measure its rendering and memory impact. Avoid enabling every size or a full CJK family merely to fix one missing character.

### Compile and declare the generated source

For this repository's `main` component, append this after the existing `idf_component_register(...)` in `main/CMakeLists.txt`; do not replace its source or dependency lists:

```cmake
target_sources(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../assets/fonts/app_font_20.c"
)
```

Alternatively, add that source to the component's existing `SRCS` list. Use only one method: do not compile the same font twice or `#include` its `.c` in another source. Include `lvgl.h` and use `LV_FONT_DECLARE(app_font_20)` in the application file or shared font header. The declaration must match the generated symbol; it does not define or link the font by itself. A separate font component must declare its LVGL dependency.

## 5. Bind text and icons without modifying read-only fonts

If `app_font_20` covers Chinese and ASCII but lacks the application's LVGL icons, it can fall back to the already enabled Montserrat 20. For these static generated fonts, keep a writable descriptor with application lifetime and initialize it once before any widget uses it:

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

Call `app_fonts_init()` first, after LVGL initialization and before creating labels. Run UI operations in the LVGL task or under `bsp_lvgl_lock()`. This shallow descriptor copy is for the static bitmap font in this example, not a general cloning method for dynamically loaded font engines. Keep the descriptor and referenced data alive for all users; never cast away `const` on an existing font or create a fallback loop.

Fallback is directional: here the application font is tried first and Montserrat supplies only missing glyphs. It cannot supply additional Chinese characters absent from both fonts. Equal nominal sizes do not guarantee matching baselines or line heights; inspect mixed lines and reserve enough height, or regenerate compatible metrics. For Latin-first typography, deliberately reverse the primary/fallback choice using an application-owned descriptor and recheck metrics and coverage.

Audit actual widget bindings, not just the screen/default font. `ui_pixel_label()` sets local fonts, `ui_pixel_screen_create()` uses Montserrat 20 for its title, and menu/demo labels select Montserrat explicitly. New applications should implement their own UI/font helpers. For compound widgets, inspect the part that draws text and any separately created popup list; check focused, pressed, checked, and disabled states too.

## 6. Prove glyph coverage and identify the active font

This helper checks a Unicode code point, not a UTF-8 byte or a multicharacter C literal:

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

For an existing label, get `lv_obj_get_style_text_font(label, LV_PART_MAIN)` under the LVGL lock, then check that font rather than an unrelated global symbol. This getter resolves the current state; repeat after each relevant state transition. A positive result checks descriptors/fallback coverage, not successful bitmap decoding or final screen output.

Decode all fixed text into Unicode code points and check the ones required by each size/style; omit control characters such as newline from printable-glyph checks. Log missing values as `U+XXXX`. For the generated subset above, U+4E2D/U+6587/U+FF0C/U+3002 should pass if the source provides them; U+9F98 is intentionally absent and should fail with the shown Montserrat fallback. Include a known-missing negative case so the coverage test cannot pass unconditionally.

Dynamic content needs an explicit contract: accept a documented character set, add verified glyph coverage, or show a deliberate supported replacement/unsupported-text message. Do not silently drop unknown characters or claim that a fixed menu subset supports arbitrary names and network text. When the supported inventory changes, regenerate fonts and rerun coverage checks.

### Static coverage gate

`./tools/validate.sh --static` runs [`tools/check_glyphs.py`](../../../tools/check_glyphs.py). It reads the code points that generated fonts under `assets/fonts/` and `main/` actually contain, from their `lv_font_fmt_txt_cmap_t` tables rather than the converter's request, and fails when a string literal in `main/` uses a non-ASCII character outside them. Each missing character is reported as `file:line: U+XXXX`. It decodes `\uXXXX` escapes, checks `#define` bodies and `snprintf` formats, and skips comments, `#if 0` blocks, `#include` paths and literals passed to `ESP_LOG*`, `printf`, `puts`, or assertions. Mark a line that is never displayed with a `glyph-check: ignore` comment. A tree without a bundled font, such as this baseline, reports `SKIPPED`.

The gate checks the union of all bundled fonts. It cannot tell which font a widget selects, does not see text assembled at runtime or received from users or the network, and does not prove the glyph renders. Keep the per-font checks above and the device inspection in section 8.

## 7. Diagnose failures in order

| Symptom | Next action |
| --- | --- |
| Undefined reference to a font | Check symbol spelling, CMake source registration, font enable macro, and a C-compatible declaration when consumed by C++ |
| Font file exists but nothing changes | Check the actual selected font; copying assets or declaring a symbol is insufficient |
| ASCII works, Chinese is blank/boxed | Check encoding, per-code-point coverage, and the actual fallback chain; do not disable placeholders |
| Body text works, title/popup fails | Inspect local fonts, widget parts, separately created children/popups, and state overrides |
| Glyph check passes but text is blank | Check the bitmap format/decoder, font lifetime, text color/opacity, hidden flags, and LVGL allocation errors |
| Characters overlap or lose their top/bottom | Check line height, baseline, parent clipping, label width/height, and fallback metrics |
| New strings fail after an otherwise successful build | Regenerate all affected subsets/sizes, inspect the actual configuration, and confirm the device is running the intended build |

## 8. Validate without overstating the result

Run the [complete repository gate](build-and-test.md) and your application's character-coverage tests. Inspect `idf.py size-components`/`idf.py size-files` for Flash/static RAM, and record runtime free heap and largest free block with the real UI and networking active. A font compiled into read-only Flash is not equivalent to allocating the whole font on the heap, but drawing/loading/caching can still require internal RAM.

On the device, inspect every page and state, mixed text/icons, long lines, different sizes, and dynamic input at both sides of the supported-set boundary. Test after page recreation and restart, not just the first label. Record the firmware version, font/converter versions, tested character inventory, screenshots where useful, and any missing characters or layout defects. Without this check, Chinese rendering remains `Unverified`; neither the baseline host tests nor a successful firmware build verifies the examples' visual output.
