<p align="right">
  <a href="grid-rows-for-optional-entities.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Grid Rows for Entities That May Not Exist Yet

Captured while adding hybrid peoples to **Dust of Worlds** (`v0.49` → `v0.50`) on
the AI Passport. A ten-entry species grid replaced a six-entry one: four of the
entries are hybrids that only exist after two base peoples have met. Opening the
species page then rebooted the board — no warning in the log, just a store fault
on the page that draws the grid. The entry is about the shape of that bug, because
it recurs whenever an array is indexed by a dense id but populated sparsely.

## The setup

Species are a dense enum; the four hybrids are simply the tail of it:

```c
typedef enum {
    SP_HUMAN = 0, SP_BEAST, SP_BIRD, SP_SCALE, SP_INSECT, SP_WOOD, SP_BASE,
    SP_HY_HUMAN_BEAST = SP_BASE, SP_HY_BIRD_SCALE, SP_HY_BEAST_BIRD, SP_HY_WOOD_INSECT,
    SP_COUNT
} species_t;
```

The grid shows two columns of five rows, position derived from the id:

```c
int col = s & 1, row = s / 2;
int x = 4 + col * 116, y = 4 + row * 30;
```

Cells for the six base peoples are built once at page creation. Hybrid cells are
built the first time that hybrid actually appears, because a row for a people that
does not exist is noise the player has to skip over.

## The bug

The refresh loop assumed every cell existed, because the layout had ten slots:

```c
label_set(s_sp_lbl[s], buf);                     /* NULL when s is a new hybrid */
lv_obj_set_style_bg_color(s_sp_dot[s], ...);     /* NULL */
```

The arrays `s_sp_dot[]`, `s_sp_lbl[]`, `s_sp_bar[]` are all-NULL for a hybrid that
has not appeared yet. The loop also ran for those ids — it had to, in order to
decide whether they should be shown — so the first refresh after entering the page
wrote through a NULL pointer and the board rebooted.

Three details made this slower to find than it should have been:

- the crash happened on **page entry**, not during simulation, so it looked like a
  page-construction bug rather than a refresh bug;
- the enum ids are dense and small, so the arrays looked fully covered;
- the base peoples worked, which made "the grid works" a reasonable first
  assumption.

## The rule that fixes it

**Only the creation path may touch a cell.** Everything else runs after a cell is
known to exist, and absence is expressed by `continue`, not by a NULL check
scattered through the drawing code:

```c
bool show = (s < SP_BASE) || gp > 0.5f;   /* base peoples always have a row */
if (!s_sp_dot[s]) {
    if (!show) continue;                  /* not created yet: nothing to draw */
    species_cell_create(s);               /* the only writer of the arrays */
}
/* from here on every cell pointer is valid */
```

Hidden is a different state from absent. A hybrid that appeared and then died out
keeps its cell and is hidden, so its row position never shifts and the layout does
not re-flow while the player is looking at it.

## Selection has the same problem

Arrow keys move a selection index over the same dense range. If they step onto an
id with no row, the highlight and the detail view point at nothing. The fix is to
make the step function itself aware of presence, and to repair the selection when
the current people disappears:

```c
static int species_step(int cur, int dir) {
    for (int k = 1; k <= SP_COUNT; k++) {
        int sp = (cur + dir * k + SP_COUNT * 2) % SP_COUNT;
        if (species_present(sp)) return sp;
    }
    return cur;                            /* one entry: stay put */
}
...
if (!species_present(s_sp_sel)) s_sp_sel = 0;
```

Keeping the highlight position derived from the dense id (`col = id & 1`,
`row = id / 2`) rather than from "position in the visible list" is what makes this
safe: cells and highlight cannot disagree, because neither is computed from a
filtered copy of the list.

## What to take away

1. **A dense id does not imply a dense cell array.** If presence is optional, the
   array is sparse no matter how the enum looks.
2. **One creation site, and a guard that returns before any other use.** A NULL
   check copied into every drawing branch is a smell; a single
   `if (!created) { if (!wanted) continue; create(); }` is the invariant.
3. **Absent ≠ hidden.** Create on first appearance and hide later, so row
   positions stay stable across a session.
4. **Guard the selection, not just the drawing.** Stepping over an absent entry is
   the second half of the same bug, and it shows up as "the detail view is blank"
   rather than as a crash.
5. **A reboot on page entry is worth reproducing with the simulation paused.** The
   fault is in the page's refresh path, and pausing the world keeps the entity
   set still while you bisect it.
