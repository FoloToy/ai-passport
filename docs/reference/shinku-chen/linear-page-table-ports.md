<p align="right">
  <a href="linear-page-table-ports.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Porting Games Whose Source Is a Linear Page Table

Several MiBand visual-novel ports ship the whole game as one flattened page table
plus a branch configuration, instead of per-scene script files. DRACU-RIOT! is one
of them: 52,787 pages, 54 choice pages, and a `branchConfig.js` holding every
"deviation from linear" (forward overrides, back blocks, conditional routes and
ending pages). Porting such a source is mostly a data-engineering job; the reader
itself is small.

## What the source gives you

- **One global page table.** Reading a page is "advance to page + 1" unless a
  table says otherwise. Choices store a target page directly.
- **A branch configuration in JavaScript.** Conditional routes are closures over a
  choice history: `(choice) => { if (choice[3113] === 2) return 3912; return 3871; }`.
- **Everything else is art and text**, already converted to the band's screen size.

## Pipeline that works

1. **Compile the branch closures into data, not code.** Parse the JS subset at
   pack time into an ordered rule list (conjunction of `choice[page] === value`
   literals, plus a fallback) and *exhaustively verify* the result by evaluating
   the original expression for every combination of the pages it touches. The
   firmware then needs no expression evaluator - just table lookups over a small
   choice-history array. Watch for the one `if (!(...))` form; inverting the chain
   keeps it to positive literals instead of expanding a complement.
2. **Pack pages and text as separate block streams** with one small scratch
   buffer, and make that buffer single-owner (see the companion entry on shared
   block buffers). Text stored as two-byte indices into a frequency-ordered
   character table gives the font generator its glyph order for free.
3. **Store page-table blocks at a fixed page count** (for example 256 pages =
   4 KB raw) so a page lookup is arithmetic, not a search.
4. **Sort the chapter list by page.** The upstream chapter list is grouped per
   route, so "next chapter" needs the page order, not the authoring order. Add
   after-story entries from the same source so the list matches what the game
   actually contains.
5. **Verify with a host walk, not with screenshots.** A deterministic walk
   ("always take option 0", plus a BFS that searches for each route's entry page
   with a real choice history) catches route loops and unreachable chapters that
   single-frame captures never show.

## Data defects worth checking for before shipping

- **Backward jumps that land in a duplicated block.** The converter can emit the
  same scene twice and route both variants' endings at the *first* copy, which
  loops the player for hundreds of pages with no choice able to escape. Detect it
  by treating the forward graph as a functional graph and asking whether every
  reachable page can still reach an ending; the same check belongs in the pack
  test suite.
- **Route-entry reachability.** Assert that each route's first page is reachable
  from page 1 with a real choice history; a mis-derived condition silently sends
  the player to the normal ending instead.
- **Chapter and after-story entries** must exist for every route the source lists.

## Reproducibility

Commit the source project's *assets and script data* (not its quick-app code) into
the fork, keep the packers' default source path pointed at that copy, and record
the exact packer arguments next to it. Then a fresh clone rebuilds byte-identical
packs without network access - which is also what makes "drop this file / change
this quality" experiments cheap to re-run.
