<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Connect Four

A landscape Connect Four for the AI Passport. Hold the device sideways and play on
a 10 × 7 board: face the computer at three difficulty levels, or pass the device
back and forth with a second player.

## Publish information

- **Title**: Connect Four
- **Description**: Turn the AI Passport into a pocket Connect Four board. Hold it
  sideways and play on a 10 x 7 grid: move the column cursor with the up and down
  keys, drop a disc with OK, and connect four to win. Play against the computer at
  three difficulty levels, or hand the device back and forth with a second player.
  Choose whether the next move is previewed at its real landing slot or only on the
  top row of that column. Column moves, drops, wins and draws each have their own
  sound, and the device dims itself when idle — press any key to keep playing.
- **Category**: games
- **Cover**: `comm_cover.png` (PNG, 1152 × 1536, 3:4) — publish metadata only; the
  cover image is not committed here.
- **Source**: <https://github.com/Shinku-Chen/ai-passport>

## What it does

- **Boots straight into the game**: the firmware opens its own settings screen,
  with no test menu and no network use.
- **10 × 7 board**: 70 slots with four-in-a-row detection horizontally, vertically
  and on both diagonals; the winning four are ringed, and a full board is a draw.
- **Two modes**: human versus computer, or two players taking turns on the same
  device with a turn indicator.
- **Three difficulty levels**: easy, medium and hard. Every move is capped by a
  search time budget, so the computer answers well inside a second, and the lower
  levels deliberately play a weak move at a fixed rate so a new player can win.
- **Selectable drop preview**: either the slot the disc will really land in, or
  only the top row of the selected column.
- **Sound**: column moves, drops, wins, losses and draws each have their own cue;
  the cues are synthesized, so no audio files are stored.
- **Idle deep sleep**: after 60 seconds on the settings screen or 180 seconds in a
  match, the device shows `SLEEPING`, powers down and sleeps; any key wakes it and
  the game restarts at the settings screen.
- **Battery readout**: live percentage in the top-right corner, turning red below
  20% and showing `--` when no reading is available.

## Interaction

Three keys drive the whole app. Hold the device with its long edge horizontal; the
"up" key is then the right-hand key.

Settings screen:

- **UP / DOWN**: pick a row (`MODE`, `LEVEL`, `PREVIEW`, `START`).
- **OK**: change the value of the selected row, or start a match when `START` is
  selected. `LEVEL` is disabled in two-player mode.

In a match:

- **UP / DOWN**: move the column cursor right or left.
- **OK**: drop a disc in the selected column.
- **OK (hold)**: return to the settings screen, discarding the current match.

After a win or a draw:

- **OK**: start another match with the same settings.
- **OK (hold)**: return to the settings screen.

Press timing: a press shorter than 120 ms is ignored, and holding a key for 300 ms
triggers the long-press action.

## Source

- Repository: `Shinku-Chen/ai-passport`, branch `feature/connect-four`
  (<https://github.com/Shinku-Chen/ai-passport/tree/feature/connect-four>),
  release `v1.6.0-connect-four`.
- Released to the community as project `community-7dc95814`.
