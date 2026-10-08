<p align="right">
  <a href="cjk-line-pitch-from-font-metrics.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Deriving CJK Line Pitch from Font Metrics, Not from the Em Size

This reference originated from the **limelight lemonade jam reader** (`v0.1.0-limelight`)
after a device test showed a five-line dialogue page overflowing its band: the last line
was drawn below the panel area that the reader owns, which on screen reads as "the
dialogue box is filled with text". The fix was verified on hardware by reading the frame
back and measuring the ink rows.

## The rule LVGL actually uses

A line's pitch in an LVGL label is

```text
pitch = font->line_height + text_line_space
```

`line_height` belongs to the font, and for CJK subsets it is usually **larger than the
size the font was generated at**: the 16 px subset generated from Noto Sans SC reports
`line_height = 20` and `base_line = 3`. Setting the style as "generated size minus the
em box", which is what the earlier readers in this fork did,

```c
/* inherited from a template, wrong for this font */
lv_obj_set_style_text_line_space(ui->body, LIME_UI_LINE_H - 16, 0);
```

therefore produced a 24 px pitch instead of the intended 20 px. Five lines then need
120 px inside a 100 px label, so the fifth line is drawn outside the box.

## The fix

Compute the spacing from the font the label actually uses, so the pitch matches the
layout constant no matter which font is installed:

```c
lv_obj_set_style_text_line_space(
    ui->body, LIME_UI_LINE_H - (int32_t)ui->font_cjk->line_height, 0);
lv_obj_set_style_max_height(ui->body, LIME_UI_LINES * LIME_UI_LINE_H, 0);
```

Add a compile-time guard so the relationship cannot regress silently — the pagination
(`lines_per_page`) and the label geometry must agree:

```c
_Static_assert(LIME_UI_LINES * LIME_UI_LINE_H + 7 <= LIME_UI_BOX_H,
               "text lines must fit the body box, including the 7 px inset");
```

## Measuring it on the device instead of trusting the layout

The reader streams frames back over serial, so the pitch can be measured rather than
estimated: reading the 240 × 214 art canvas plus the band and counting the rows that
contain text ink gives the pitch directly. Recorded on this board:

| | ink rows (frame coordinates) | pitch |
| --- | --- | --- |
| before the fix | 220–235, 244–259 | **24 px** |
| after the fix | 220–235, 240–255, 260–275, 280–295, 300–315 | **20 px** |

The second row is the acceptance evidence: five lines at 20 px occupy 100 px, the last
one ends at row 315, and nothing is drawn outside the band (which covers 214–319).

## Where this pattern comes from

The hardcoded `- 16` was copied from an earlier visual-novel reader in this fork, whose
font happens to be generated the same way and whose dialogs are short enough that the
overflow rarely showed. Treat "generated font size" and "font line height" as different
numbers whenever a CJK subset is involved, and prefer a static assert tying the page
layout to the label box.
