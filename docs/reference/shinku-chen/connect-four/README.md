<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Connect Four

A landscape Connect Four for the AI Passport. Hold the device sideways and play on
a 10 × 7 board: face the computer at three difficulty levels, or pass the device
back and forth with a second player.

## Publish information

- **Title**: Connect Four
- **Description**: submitted as:

  > Turn the AI Passport into a pocket Connect Four board.
  >
  > Hold it sideways and play on a 10 x 7 grid: move the column cursor with the up and down keys, drop a disc with OK, and connect four to win.
  >
  > - Choose who starts: play first yourself, or let the computer open the game and watch it move.
  > - Play against the computer at three difficulty levels: pick the easy one for a relaxed win, or the hard one for a real match.
  > - Or hand the device back and forth: two players take turns on the same board.
  > - Choose how your next move is previewed: show exactly which slot the disc will land in, or only mark the top row of that column.
  > - Column moves, drops, wins and draws each have their own sound, and the device dims itself when idle - press any key to keep playing.
  >
  > Hold OK to return to the settings screen at any time and switch mode, difficulty or preview style.

- **Category**: games
- **Cover**: `comm_cover.png` (PNG, 1152 × 1536, 3:4) — publish metadata only; the
  cover image is not committed here.
- **Source**: <https://github.com/Shinku-Chen/ai-passport>

## What it does

- **Boots straight into the game**: the firmware opens its own settings screen,
  with no test menu and no network use.
- **10 × 7 board**: 70 slots with four-in-a-row detection horizontally, vertically
  and on both diagonals; the winning four are ringed, and a full board is a draw.
- **Three modes**: `HUMAN vs AI` (you move first), `AI vs HUMAN` (the computer
  opens the game and moves immediately), or `TWO PLAYERS` taking turns on the same
  device with a turn indicator. The two computer modes differ only in who moves
  first.
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
  selected. The `MODE` values are `HUMAN vs AI`, `AI vs HUMAN` and `TWO PLAYERS`;
  `LEVEL` is disabled in two-player mode.

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
