<p align="right">
  <a href="lvgl-pool-budget-without-psram.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# LVGL Memory Pool Budgeting on ESP32-C3 (No PSRAM)

Captured after the **Faraway** application shipped its first release
(`v1.0.0-faraway`, upstream baseline `1051209`). The application keeps roughly
80–110 LVGL objects resident and adds a 24-particle weather layer on top, which
makes the LVGL pool — not the system heap — the resource that decides whether a
page can be built at all. These notes apply to any AI Passport application whose
screens are large enough to matter.

## Log the pool monitor from boot; without it you are guessing

The LVGL heap is a separate static pool sized by
`CONFIG_LV_MEM_SIZE_KILOBYTES` (80 on this application), and it is carved out of
DRAM that Wi-Fi and task stacks also want. On ESP32-C3 there is no PSRAM to
borrow from, so the only way to know the margin is to measure it. Print a
monitor line at every state transition worth caring about — boot, and both
directions of the largest overlay:

```c
static void wx_mem_log(const char *why)
{
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    ESP_LOGW("fa_wx", "%-12s LVGL free=%u biggest=%u frag=%u%% maxused=%u | sys=%u",
             why, (unsigned)m.free_size, (unsigned)m.free_biggest_size,
             (unsigned)m.frag_pct, (unsigned)m.max_used,
             (unsigned)esp_get_free_heap_size());
}
```

A real capture from the device, edited for width:

```text
W (2590)  fa_wx: boot        LVGL free=8044 biggest=8044 frag=0% maxused=65608 | sys=131484
W (2828)  fa_wx: overlay-ON  LVGL free=8420 biggest=8060 frag=5% maxused=68096 | sys=131484
W (6299)  fa_wx: overlay-OFF LVGL free=8424 biggest=8060 frag=5% maxused=68492 | sys=131616
W (168727) fa_wx: overlay-OFF LVGL free=8052 biggest=7576 frag=6% maxused=68492 | sys=131484
```

## Read the columns correctly — `maxused` is the margin, not `free`

- **`maxused`** is the peak since boot and the only column that answers "how
  far from the wall am I". A pool that looks roomy (`free` in the thousands)
  can still be the pool that dies on the next page.
- **`free_biggest_size`**, not `free_size`, decides whether the next large
  allocation succeeds: several small fragments can sum to a comfortable `free`
  while no single block is large enough. Watch `biggest` for the pool's ability
  to serve one more full-screen transform buffer.
- **Do not add `free` and `maxused` to derive the pool total.** The two are
  measured on different bases and the arithmetic does not close — on this
  capture `80 KB − free = 73876`, which is larger than a `maxused` of 68492,
  which is impossible for a true high-water mark. Treat `maxused` as a trend
  and `free_biggest_size` as an allocation gate; read the configured pool size
  from `CONFIG_LV_MEM_SIZE_KILOBYTES` instead of backing it out of the monitor
  line.
- **`sys`** is the IDF system heap. `CONFIG_LV_MEM_SIZE_KILOBYTES` moves memory
  between this pool and Wi-Fi/task stacks, so raising one column lowers the
  other; always read them together.

## Sample long enough to separate a peak from a leak

A single reading cannot tell a peak from a slow leak. Take the monitor line on
both edges of the heaviest screen repeatedly and compare `maxused` across the
run: on this application it reached its high-water mark of **68492 B within the
first 6.3 seconds** and stayed exactly flat for the remaining **162 seconds**
across 30 samples, while `free` drifted between 7900 and 8424 and
`free_biggest_size` between 7576 and 8092. A flat `maxused` over a long run is
the evidence of no leak; a drifting one is the alarm. Build the heavy page,
leave it, and come back — the weather layer alone was worth adding 29 resident
objects during development, and the later keepsake album another 25.

## When the pool runs out the screen freezes, it does not crash

This is the failure signature worth memorizing, because it looks like a hang and
is easy to misattribute to the display driver. LVGL 9 has no pool expansion when
`CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES` is 0: `lv_malloc` fails,
`LV_ASSERT_MALLOC` fires the default handler (`while(1);`), and the render task
stops mid-frame with the backlight already on. The result is a **frozen,
half-drawn frame — no reboot, no panic, and no serial output** because
`LV_USE_LOG` is off. If a page freezes the instant it opens and the log is
silent, suspect the pool before the panel.

Two ways out, with different costs:

- **Raise `CONFIG_LV_MEM_SIZE_KILOBYTES`.** Least invasive and keeps the static
  pool guarantee, but the memory comes straight out of the system heap.
- **Switch LVGL to the C-library allocator** (`CONFIG_LV_USE_CLIB_MALLOC=y`),
  which folds the fixed pool into the general heap and removes the ceiling
  entirely, at the cost of losing the isolation between rendering and Wi-Fi
  allocation. See the related entry on the meter UI for that route.

## Generalization for the next app

- Log `lv_mem_monitor()` from boot; a pool you never measure is a pool you
  cannot budget.
- Judge margin by `maxused`, allocation success by `free_biggest_size`, and
  pool size by the sdkconfig value — never by arithmetic across monitor
  columns.
- Sample the heaviest screen repeatedly before calling it safe; only a flat
  `maxused` over a long run rules out a leak.
- A frozen half-drawn frame with a silent log is pool exhaustion until proven
  otherwise, not a display problem.
- Every new page's object count is a pool decision, not just a UI decision.

## Related documents

- `docs/reference/starsms007/faraway/README.md` — the application these
  measurements come from.
- `main/fa_view.c` — the `wx_mem_log()` helper and the build-time reasoning
  around `build_scene()`.
- `sdkconfig.defaults` — `CONFIG_LV_MEM_SIZE_KILOBYTES`, with the note on why
  it was raised.
- `docs/reference/y2lin/meter-ui-smoothing-and-layout.md` — LVGL pool
  exhaustion seen from the other side, as a white screen at boot, and the
  `CONFIG_LV_USE_CLIB_MALLOC` remedy.
- `docs/reference/shinku-chen/display-refresh-and-deep-sleep.md` — display
  refresh behavior on the same no-PSRAM target.
