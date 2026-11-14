<p align="right">
  <a href="rotated-full-screen-map-page.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# A Rotated Full-Screen Map Page on a 240 × 320 Handheld

Captured while adding the fifth page — a full-screen atlas — to **Dust of Worlds**
on the AI Passport (`v0.46` and `v0.47`). The board is an ESP32-C3 with 8 MB of
flash, no PSRAM, a 240 × 320 portrait panel and three keys. Two things went wrong
in a way that reads as "the device froze": the image was drawn unrotated at 1:1 in
a corner, and switching to the page locked the UI until reboot. Both are worth
knowing before adding a page that is one big picture.

## Rotate the index, not the pixels

The world map is rendered by the UI itself into a 208 × 126 RGB565 buffer
(52 KB, one buffer). For the full-screen page the same buffer has to appear
turned 90° clockwise and stretched over the whole panel.

The cheap way to do that is not a rotation pass and not an extra buffer. It is to
make the writer and reader disagree about the layout:

```c
static inline int map_index(int x, int y) {
    return s_map_rot ? (x * MAP_H + (MAP_H - 1 - y)) : (y * MAP_W + x);
}
```

With `s_map_rot` set, every plot lands in column-major order, so the finished
buffer already reads as 126 wide × 208 tall when described that way to LVGL:

```c
dsc.header.w = MAP_H;   /* 126 */
dsc.header.h = MAP_W;   /* 208 */
```

Then the image object stretches it to 240 × 320 with
`LV_IMAGE_ALIGN_STRETCH` and antialiasing off. Costs and benefits:

- no second buffer, no rotate pass, no tear between the two pages;
- the stretch is non-uniform (126 → 240 horizontally, 208 → 320 vertically), so
  the map is slightly distorted — fine for a schematic world, not fine for a photo;
- one invalidation of the stretched object covers 240 × 320 = 76,800 px instead of
  208 × 126 = 26,208 px, so this page is about three times as expensive per
  redraw as the map area it came from. That number decides the refresh policy
  below.

## Trap: `lv_image_set_src` resets size and alignment

The first build showed the map at its natural size in the top-left corner, cut
off. The cause is ordering: in LVGL 9, setting the source re-applies the image's
own size and inner alignment. Setting size and alignment first and the source
afterwards silently undoes both.

```c
mapfull_dsc_init();                                   /* set src first */
lv_obj_set_pos(img, 0, 0);
lv_obj_set_size(img, SCR_W, SCR_H);
lv_image_set_inner_align(img, LV_IMAGE_ALIGN_STRETCH);
lv_image_set_antialias(img, false);
```

Nothing in the API name suggests a side effect on sibling properties; the symptom
(an image drawn 1:1 in a corner) looks like a layout bug, not an ordering bug.

## Trap: a full-screen image under animated overlays freezes the board

The atlas page was built like the other four pages: a background box plus the
page's own decorations. On this page the decorations were the shared ones —
event banner, starlight, reveal window — all of which animate above the content.

The symptom was not a crash and not a watchdog reset: entering the page stopped
responding to keys until the device was reset. The first suspicion was the
simulation. It was the compositor: every banner or starlight animation invalidates
a large region that now sits on top of a stretched full-screen image, so the UI
task re-composites most of the panel per frame, and the four-per-second page
refresh added its own full-surface invalidation on top. On a part with no PSRAM
and no GPU, that is enough to starve everything else the UI task owns.

The fix is a rule, not a tuning constant: **a pure-display page carries no
animated chrome.**

- Every overlay is suppressed while the page is current:
  `if (best_t && s_page_idx != 4 && ...) overlay_show(...)`, same for the
  starlight and the wonder reveal.
- Anything that *needs an answer* — a prayer, a ruler's edict, a character card,
  the wake report — first switches back to the world page and opens there:
  `if (s_page_idx == 4) set_page(0);`. A modal never floats over the map, so the
  "pure" page stays pure by construction rather than by discipline.
- The page's own help line is hidden instead of saying something that the other
  pages do not say.

## Refresh at 1 Hz, and never invalidate the page underneath

Once the overlays were gone the page still refreshed four times per second,
because it shared the world map's update path. The map does not change that fast:
tribes move on a scale of ticks, and at 1× a tick is 600 ms.

```c
static void refresh_map_full(void) {
    uint32_t now = lv_tick_get();
    if (s_full_ms && (uint32_t)(now - s_full_ms) < 1000) return;   /* 1 Hz */
    s_full_ms = now;
    s_map_rot = true;
    s_map_no_invalidate = true;    /* do not invalidate the hidden world map */
    redraw_map();
    s_map_no_invalidate = false;
    s_map_rot = false;
    lv_obj_invalidate(s_mapfull_img);
}
```

Two separate savings: the redraw happens at most once a second, and redrawing the
buffer no longer invalidates the world map object that is invisible behind the
atlas page. Without the second flag the renderer still walks a hidden full-size
object on every redraw.

## What to take away

1. **Rotate with the index, not with the pixels.** One buffer, one describing
   call, no rotate pass — as long as a non-uniform stretch is acceptable.
2. **Set the image source before size and alignment.** LVGL 9 re-applies image
   properties when the source changes; the resulting layout looks like a bug
   somewhere else.
3. **Do not put animated chrome on a full-screen image page.** On a no-PSRAM part
   the composition cost of the overlay is the whole panel, per frame. Make the
   page structurally pure and route anything modal back to a page that can host
   it.
4. **Price a page in invalidated pixels, not in objects.** 76,800 px per refresh
   is the number that decides whether 4 Hz is affordable — here it is not.
5. **Freeze with live keys is a timing bug, not a logic bug.** If a page switch
   stops responding but the device never reboots, look at what the UI task is
   being asked to redraw, before looking at the simulation.
