<p align="right">
  <a href="persistent-world-simulation-on-a-handheld.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Keeping a Persistent World Alive on a Three-Key Handheld

Captured after **Dust of Worlds** shipped `v0.27` to the community
library as revision `5371`. It is a god-view world simulator on the AI Passport:
240 × 320 screen, 8 MB flash, no PSRAM, three keys, and a world that keeps
evolving while the player only watches. Two problems took the most iteration —
matching a compressed calendar to population growth, and keeping recurring
disasters from turning into a permanent death spiral. Both are easy to reproduce
in any long-running simulation on this board.

## A compressed calendar needs a growth rate that matches it

The world advances **100 years per tick**. That choice is invisible in the UI
until it collides with the population model. The first version grew each tribe by
about `0.38%` per tick, which is a plausible *per-generation* rate — but a tick is
a century, several generations. The result was a world that displayed millions of
years while still holding only a few thousand beings.

The fix was not a bigger cap; it was making the rate honest for the tick length:

- growth per tribe rose to about `3%` per tick, still damped by a soft pressure
  term near carrying capacity;
- carrying capacity scales with economy, not a constant: `tech * 400 + cul * 80`
  per tribe, on top of the land and tier terms;
- there is no hard population cap; density only slows growth and adds a small
  crowd loss, so the continent can keep climbing after each collapse.

Measured with a host harness (100 years/tick):

```text
 30k ticks (~3M years): 0.7M – 5.5M beings, peaks near 14.7M
160k ticks (~16M years): 2.07M and 1.92M on two seeds
```

Two practical rules came out of this:

1. **Pick the calendar first, then derive the rate.** If a tick is Y years and a
   generation is G years, the per-tick growth is roughly `(1 + r_generation)^(Y/G)`.
   A rate copied from a one-generation model will be wrong by that exponent.
2. **Scale capacity with the civilization, not with a constant.** A fixed cap makes
   a million-year world look like a village forever; a tech-driven soft capacity
   lets the same code produce thousands early and millions later.

## Recurring percentage damage needs an anti-snowball

The main crisis is a periodic insect swarm. Each swarm spawns 6–11 groups, takes
30–55% of every tribe on arrival, and then raids nearby tribes for up to 15% per
four ticks while a blight of several hundred ticks cuts growth and food. That is
dramatic — and it produced a death spiral:

1. a swarm kills tribes;
2. a dead tribe takes its technology with it;
3. a new tribe is founded from a group with near-zero tech;
4. lower tech means lower capacity and lower defence;
5. the next swarm arrives before the new tribe catches up.

A trace made the loop obvious: 10.9M beings at tick 10k, zero at tick 20k, a
handful at 30k, millions again by 40k, then another crash. The world was not
being killed by one swarm; it was being repeatedly reset.

Four mechanisms broke the loop. They are worth copying as a pattern, not as
specific numbers:

- **Civilization memory.** Record the highest technology any tribe has reached;
  when a new tribe is founded, start it at 35% of that peak. Knowledge survives a
  collapse. Persist the value in the save.
- **Accumulating resistance.** Each swarm wiped out raises resistance by 5 (and a
  last stand by another 10), capped at 80. Defence is
  `0.7 * resistance + min(avg_tech / 3000, 0.5)`, capped at 0.9, and it scales both
  the arrival damage and the raid damage. Early swarms are lethal; later ones are
  survivable without making the world invulnerable.
- **A last stand with a duration.** When beings fall to 1% of the historical peak
  during a blight, give the world 300 ticks of 95% defence, 1.5× growth and
  counter-kills against nearby insects, and clear the blight. It is a comeback
  window, not a permanent buff.
- **A seed floor.** For 15k ticks after a swarm, if beings fall below 300, revive
  or create one 600-being group and restart the last stand. The world can still
  lose almost everything, but the insects cannot reduce it to literal zero.

A 35% chance for the random oracle to strike the insects directly adds a second
escape valve without giving the player a button.

Results after the changes:

```text
30 seeds × 20k ticks: 0 total extinctions
final populations:    0.6M – 12M
160k ticks:           millions of beings, no zero
```

The general lesson: **any recurring percentage loss that also destroys the
thing that produces defence will eventually win.** Pair it with memory,
accumulating resistance, a timed comeback and a small floor — or the late game
becomes a reset loop rather than a crisis.

## A save format for state that keeps growing

The world saves tribes, groups, terrain and a chronicle. Fixed-size records make
the layout predictable and the load bounded:

- tribe record: 60 bytes; group record: 27 bytes; chronicle entry: 79 bytes;
- terrain type and fertility are 4-bit packed;
- the load reads in two phases: first validate counts and record sizes against
  the buffer, then decode; a truncated save is rejected before it can scribble
  over the live world;
- new fields go into an **optional tail** (cooldown slots, ice baseline, bug
  resistance, technology memory). A save without the tail still loads with
  defaults; only a layout change that breaks the fixed records bumps the save
  version.

A truncation sweep (every prefix length of a valid save) is cheap and catches
the "load a short buffer" class of bugs before a release. The NVS partition is
sized generously (88 KB) so the save and the settings namespace do not fight.

## Put the long run in a host harness

Two host programs paid for themselves many times:

- a **seed sweep** (30 seeds × 20k ticks) that reports final population, min/max,
  tribe count and any NaN;
- a **trace run** (160k ticks) that prints population, tribe count, average tech,
  resistance, blight and insect count every 10k ticks.

They run in seconds on a desktop, catch both "never grows" and "always dies"
regressions, and make balance changes reviewable with numbers instead of feel.
The trace is also the fastest way to see a death spiral that a single final
population number hides.

## Takeaways

- Derive per-tick growth from the tick length; do not reuse a per-generation rate.
- Scale capacity with civilization, keep the pressure soft, and avoid a hard cap.
- Recurring percentage damage needs civilization memory, resistance, a timed last
  stand and a seed floor, or it becomes a reset loop.
- Keep the save layout fixed and put new state in an optional tail; test truncation.
- Keep a seed sweep and a long trace in the release checklist.
