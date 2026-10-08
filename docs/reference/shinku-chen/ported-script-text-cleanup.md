<p align="right">
  <a href="ported-script-text-cleanup.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Cleaning a Ported Script Dataset Before It Reaches the Reader

This reference originated from the **limelight lemonade jam reader** (`v0.1.0-limelight`),
whose upstream dataset is a hand-edited conversion of a commercial script. 111 of its
68,229 dialogue lines carried text that is not dialogue; rendered literally they look
like a broken story rather than a formatting problem. The cleaning runs in the packer,
so the reader and the firmware never see it.

## What was in the data

- **Inline layout directives (104 lines)**: `%f` and `%r` switch a font face and reset
  it, `$name$` (or `$name` without a closing marker) inserts a named image, and
  `#rrggbbaa` sets a colour, so a line can read `text %f$heart$#00ffadd6X%r more text`.
  Only the directives should go; the visible `X` between them is real text.
- **Leaked translator memos (7 lines)**: a `memo:` note with the original Japanese and
  an English explanation was pasted into the line itself, in three spellings
  (`memo:`, `memo :`, `memo「:」`). The longest one is 740 bytes, which paginates to six
  screens of English prose inside the dialogue box.

## Rules that survived contact with the data

- **Clean in the packer, never in the reader.** The reader only ever sees UTF-8 text, so
  the cleaning rules stay testable on the host and cost the firmware nothing.
- **Match paired directives before single ones.** `$name$` and `$name` need different
  patterns, and the paired one has to win: otherwise `$ハート$#00ffadd6` leaves a stray
  `$` behind.
- **Never eat ordinary punctuation.** A first attempt with `\$[^\s]{1,16}` also consumed
  the `$5` in a line like `the strings cost $5 each`, silently changing dialogue. Requiring the name to start
  with a letter, kana or ideograph (`\$(?=[^\W\d_])[^\W\d_]{2,16}`) fixes it, and that
  case is now a regression test.
- **Self-check what is left.** After cleaning, scan for anything that still looks like a
  directive (`$` followed by a letter, `#rrggbb`, `%` plus a letter) and count it. A new
  marker style then shows up as a number in the packer's report instead of in a
  screenshot.

## What it changed

| | before | after |
| --- | --- | --- |
| dialogue lines needing a second page | 20 | 13 |
| maximum pages for one line | 6 | 2 |
| longest line | 740 bytes | 240 bytes |

A sweep for lines that are mostly ASCII letters (≥ 60 %) was used to look for further
leaks: the five remaining hits are legitimate content — a venue name, an "OK", a band
name and two romanised onomatopoeia lines.

## Where this applies

Any port whose dataset was translated, typeset or edited by hand can carry this kind of
residue, and a reader has no way to tell it apart from dialogue. Budget a pass over the
whole script: count the markers, look for note-like prefixes, and keep a regression test
for every false positive the cleaning rules produce.
