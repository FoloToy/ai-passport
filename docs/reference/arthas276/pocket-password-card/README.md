<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Pocket Password Card

An offline derived-password card: the device keeps one fixed 8 × 10 character
grid derived from a 128-bit seed, and every site is just a position on that
grid. No password is ever stored, nothing goes online, and the same seed
rebuilds the identical card on any unit.

## Publish information

- **Title**: Pocket Password Card (published in the community library under its
  Chinese title, recorded in the [Simplified Chinese peer](README.zh_CN.md))
- **Community listing**: <https://ai-passport.folotoy.cn/plays/?slug=community-008b1882>
- **Published description**:

  Pocket password card: a 128-bit seed generated on first boot derives a fixed
  8x10 character grid. Every site stores only row + column + length + mode, and
  its password is re-derived from the same card stream on demand - no password
  is ever stored, nothing goes online, and the same seed rebuilds the identical
  card on any unit. Passwords stay masked by default; OK reveals for 8 seconds
  then re-masks, and any other key masks instantly. Site names are typed on a
  three-key wheel. Sites can be deleted and edited (long-press UP deletes with
  double-confirm, long-press DOWN reopens the wizard prefilled - both hinted on
  screen); password length ceiling is 16 characters; a PIN mode derives 4-6
  pure digits from the same seed and never appears on the printable card, so a
  copied wallet card leaks no bank PINs; the seed view gains a CRC-8 checksum
  and settings gains on-device seed entry. A 55-character unambiguous alphabet
  gives ~46 bit (8 chars) to ~92 bit (16 chars) of in-card entropy -
  shoulder-surfing and casual-flip protection, not a password manager. Existing
  v1.0 data migrates automatically and every old password derives
  bit-identically.

## What it does

- **One card, derived not stored**: first boot generates a 128-bit seed with
  `esp_fill_random`; `xoshiro128**` expands it into a fixed 8 × 10 grid over a
  55-character unambiguous alphabet (no 0/O/o, 1/l/I/i). The seed lives in one
  NVS blob and never leaves the device.
- **A site is row + column + length + mode** (13 bytes): the password is the
  run of that many cells starting at that position on the 80-cell stream,
  wrapping past the end. 4-16 characters for standard passwords.
- **PIN mode** (v1.2): 4-6 digits from a domain-separated stream keyed by the
  same seed, the site position and a "PIN1" tag - rebuildable on any unit but
  never printed on the card, so a stolen or photographed wallet card leaks no
  bank PINs.
- **Masked by default**: OK reveals for 8 seconds with a shrinking countdown
  bar, then re-masks itself; any other key masks instantly.
- **Site management** (v1.2): long-press UP on the password screen deletes the
  site through a double-confirm; long-press DOWN reopens the wizard prefilled
  for in-place editing. Both gestures are stated in on-screen hint lines.
- **Migration without a host tool** (v1.2): the seed view shows the 32 hex
  characters plus a CRC-8 checksum; settings offers on-device seed entry on a
  0-9 a-f typing wheel with a live counter and candidate checksum, so a
  single-character typo shows up as a checksum mismatch instead of silently
  producing a different valid card.
- **Store v2 with automatic v1.0 migration**: the 440-byte blob (mode byte per
  site) converts a 408-byte v1.0 blob in one pure, host-tested step on load and
  self-heals the flash copy; v1.0 passwords derive bit-identically.
- **Idle policy**: 12 s backlight dim, 60 s deep sleep with button wake and the
  repo key-guard anti-accidental-press contract.

## Interaction

Three keys only: UP/DOWN move and accelerate (hold to repeat), OK confirms;
long-press OK is a consistent per-screen back. Nine screens: the card grid
(column headers A-J, row numbers 1-8, cursor row highlighted), the site list
(with a fixed add-site row), the password screen (masked/revealed), a five-step
add/edit wizard (mode, row, column, length, name typing wheel), settings
(regenerate card, view seed, restore seed, clear sites, help), quad-confirm,
seed view, seed entry, and a first-boot help overlay. Every screen footer
states the current gestures.

## Source

- Repository: `arthas276/ai-passport`, branch `feature/pwcard`
  (<https://github.com/arthas276/ai-passport/tree/feature/pwcard>).

## Cover

- Cover recorded as `cover.png` (PNG, 1152 × 1536, 3:4) — publish metadata only,
  the archive is text-only.
