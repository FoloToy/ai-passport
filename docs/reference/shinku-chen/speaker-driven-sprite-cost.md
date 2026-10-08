<p align="right">
  <a href="speaker-driven-sprite-cost.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# What "the Sprite Follows the Speaker" Costs

Captured while releasing the *Starry Sky Railroad and Shiro's Journey* reader.
A visual novel wants the character who is talking to be the one on screen. That is
a product rule, and on this board it has a price: every time the visible sprite
changes, the renderer has to rebuild the whole 240 × 320 frame — background JPEG
decode included — because the sprite is composited into the same canvas as the
background. This entry records the measured price of the strict rule, so the next
port can decide with numbers instead of discovering them on the device.

## The rule and the data behind it

- A sprite is drawn only while the character it belongs to is the one speaking.
  Narration, other speakers and the ending hide it; an "unknown owner" hides it too.
- The packer derives the owner from which named speaker references a sprite (the
  statistics live in the pack): 15 sprites over 7 owners for this title, and 105
  backgrounds of which 69 are flagged "never draw a sprite on me" (event
  illustrations and solid-colour scenes). Doing this at pack time is what makes the
  runtime rule one comparison instead of a filename heuristic.
- Over the whole script: **3,444 of 11,777 dialogue steps draw a sprite**; the rest
  hide it because of narration (3,606), another speaker (2,465) or a flagged
  background (224).

## What it costs

- The compositor rebuilds the frame whenever the visible sprite or the background
  changes: background JPEG decode (130–170 ms here) → text band → sprite → scrim.
  Walking the story, that happens **5,837 times over 12,968 steps — about 45 %**,
  with roughly 1,191 of those coming from scene/background changes.
- So with the strict rule, reading costs an extra full recomposite on something
  like every second speaker hand-off. Lines inside one character's speech keep the
  same sprite and stay cheap; it is the toggling that is expensive.
- The obvious fix — cache the clean background under the sprite rectangle and
  repaint only that rect — needs up to 168 × 252 × 2 = 84 KB (the sprite bound this
  title uses). That is the same 84 KB sprite decode buffer the ATRI-style port
  deliberately removed to leave headroom, so on an 8 MB, no-PSRAM board it does not
  fit. The rule and the memory budget are the same decision.

## Knobs, in the order worth trying

1. **Keep it sticky inside a scene.** Show the sprite from the first line that
   names its owner until the scene ends; only backgrounds and CG flags change it.
   This is what the source port does, and it removes most of the toggling.
2. **Shrink the sprite bound.** The cache size is `w × h × 2`; a 120 × 180 bound
   (43 KB) makes the cache affordable and the sprite smaller.
3. **Only repaint the toggling rect** if a clean-base cache exists at all; without
   one, a full recomposite is the only correct option, and a partial repaint would
   need the original pixels back.

Decide this before building the interface: the difference between "sprite follows
the speaker" and "sprite stays until the scene changes" is tens of milliseconds per
line on this hardware, and it is invisible in a screenshot.

## Related

- [`on-device-game-ai-wall-clock-budget.md`](on-device-game-ai-wall-clock-budget.md)
  — the same habit of pricing a feature in wall-clock terms before shipping it.
- [`display-refresh-and-deep-sleep.md`](display-refresh-and-deep-sleep.md) — panel
  refresh costs and what a full-frame redraw involves on this board.
