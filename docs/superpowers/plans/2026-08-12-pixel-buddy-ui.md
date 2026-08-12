# Pixel Buddy UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace all 18 ASCII pets with clipped programmatic pixel sprites and make the 240 × 320 UI obey fixed layout, overlay, and text boundaries.

**Architecture:** Add host-testable I4 drawing, text-layout, and sprite-renderer modules. Keep `buddy_ui.c` responsible only for composing fixed screen regions from a snapshot and drawing one priority overlay. Render sprites directly into the existing I4 framebuffer without heap allocation or LVGL drawing tasks.

**Tech Stack:** C17, C++ only for removal of the old imported art, ESP-IDF 5.5.3, LVGL 9.5 I4 canvas, host CMake/CTest.

## Global Constraints

- Preserve BLE protocol, security behavior, state machine, controls, and the 18-species setting.
- Screen size is exactly 240 × 320: status 26 px, pet stage 132 px, information 138 px, action bar 24 px.
- Sprite design grid is 64 × 64 and every write is clipped.
- Use no heap allocation, raster assets, GIFs, PSRAM, or image decoding.
- Hardware validation remains NOT RUN until performed on a physical board.

---

### Task 1: Clipped I4 Surface Primitives

**Files:**
- Modify: `main/buddy_i4.h`
- Modify: `main/buddy_i4.c`
- Modify: `tests/test_buddy_i4.c`

**Interfaces:**
- Produces: `buddy_i4_surface_t`, `buddy_i4_clip_t`, `buddy_i4_fill_rect()`, `buddy_i4_line()`, and bounded pixel access.

- [ ] **Step 1: Write failing clipping tests**

Add assertions that rectangles crossing each edge touch only pixels inside the supplied clip and framebuffer, and that lines never modify guard bytes surrounding the framebuffer.

- [ ] **Step 2: Run the focused test and verify RED**

Run: `cmake --build build-host --target test_buddy_i4 && ./build-host/test_buddy_i4`
Expected: compilation fails because surface and clipping APIs do not exist.

- [ ] **Step 3: Implement bounded primitives**

Define a surface with pixel pointer, width, height, and stride; clamp all primitives to the intersection of framebuffer and active clip. Preserve the existing packed-nibble helpers for compatibility.

- [ ] **Step 4: Run focused test and verify GREEN**

Run: `cmake --build build-host --target test_buddy_i4 && ./build-host/test_buddy_i4`
Expected: PASS with guard bytes unchanged.

### Task 2: Eighteen Pixel Sprite Renderer

**Files:**
- Create: `main/buddy_sprite.h`
- Create: `main/buddy_sprite.c`
- Create: `tests/test_buddy_sprite.c`
- Modify: `main/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: `buddy_i4_surface_t` and `buddy_i4_clip_t` from Task 1.
- Produces: `BUDDY_SPRITE_SPECIES_COUNT`, `BUDDY_SPRITE_STATE_COUNT`, `buddy_sprite_name()`, `buddy_sprite_render()`, and `buddy_sprite_bounds()`.

- [ ] **Step 1: Write failing coverage and bounds tests**

Loop over 18 species, seven states, and several ticks. Assert every name is non-empty, every render produces non-background pixels, bounds stay inside 64 × 64, animation frame dimensions remain within the species envelope, and framebuffer guards remain intact.

- [ ] **Step 2: Run the focused test and verify RED**

Run: `cmake --build build-host --target test_buddy_sprite`
Expected: compilation fails because `buddy_sprite.h` does not exist.

- [ ] **Step 3: Implement shared anatomy and all species**

Implement clipped pixel helpers for eyes, heart, limbs, body silhouettes, and effects. Define all existing species by explicit shape kind, body/accent palette indices, face parameters, and decorations. Map seven semantic states to deterministic breathing, blink, work, alert, bounce/confetti, dizzy, and heart keyframes.

- [ ] **Step 4: Run focused test and verify GREEN**

Run: `cmake --build build-host --target test_buddy_sprite && ./build-host/test_buddy_sprite`
Expected: PASS for all 126 species/state combinations.

### Task 3: Text Layout and Fixed Screen Composition

**Files:**
- Create: `main/buddy_text_layout.h`
- Create: `main/buddy_text_layout.c`
- Create: `tests/test_buddy_text_layout.c`
- Modify: `main/buddy_ui.c`
- Modify: `main/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`

**Interfaces:**
- Consumes: clipped I4 primitives and sprite renderer.
- Produces: bounded word wrapping with ellipsis and a UI using exact fixed region rectangles.

- [ ] **Step 1: Write failing wrap and layout tests**

Assert wrapping breaks at spaces, over-wide words end with three dots, line limits are enforced, and computed status/stage/info/action rectangles exactly tile 240 × 320 without overlap.

- [ ] **Step 2: Run focused test and verify RED**

Run: `cmake --build build-host --target test_buddy_text_layout`
Expected: compilation fails because the layout API does not exist.

- [ ] **Step 3: Implement bounded line composition**

Return line slices into caller-owned fixed buffers, honor pixel-width callbacks, and append ASCII `...` on truncation. No source string is modified and no allocation occurs.

- [ ] **Step 4: Recompose base pages**

Use status `[0,26)`, stage `[26,158)`, info `[158,296)`, and action `[296,320)`. Center sprites using reported bounds, draw names above their bounds, right-align values, and clip every section independently.

- [ ] **Step 5: Run focused and UI-independent tests**

Run: `cmake --build build-host --target test_buddy_text_layout test_buddy_sprite test_buddy_i4 && ctest --test-dir build-host --output-on-failure`
Expected: all tests PASS.

### Task 4: Overlay Priority, Removal of ASCII Art, and Firmware Verification

**Files:**
- Modify: `main/buddy_ui.c`
- Modify: `main/CMakeLists.txt`
- Modify: `tests/CMakeLists.txt`
- Modify: `README.md`
- Remove from production build: `main/buddy_art.cpp`, `main/buddies/*.cpp`

**Interfaces:**
- Consumes: completed base layout and sprite API.
- Produces: exactly one opaque priority overlay and a firmware with no linked ASCII renderer.

- [ ] **Step 1: Add overlay geometry assertions**

Test the priority order confirmation/pairing > approval > menu and assert modal header/body/footer rectangles are disjoint and remain above the action-bar boundary.

- [ ] **Step 2: Implement dimming and opaque panels**

Paint one selected overlay after the base page. Clear/dim its full coverage before painting the panel; give approval at most five wrapped hint lines and a fixed action footer.

- [ ] **Step 3: Remove old renderer from production and documentation**

Remove imported C++ sources from `main/CMakeLists.txt`, update README wording from ASCII art to programmatic pixel sprites, and ensure `rg 'buddy_art_render' main/buddy_ui.c` returns no matches.

- [ ] **Step 4: Run fresh verification**

Run: `cmake -S tests -B build-host && cmake --build build-host && ctest --test-dir build-host --output-on-failure && idf.py build && idf.py size && git diff --check`
Expected: all host tests PASS, ESP-IDF build and size PASS, diff check returns no output, and application partition retains positive headroom.

- [ ] **Step 5: Generate the 0x0 image**

Run esptool `merge_bin` with the bootloader at `0x0`, partition table at `0x8000`, and application at `0x10000`; output `build/FoloToy-Card-0x0.bin` and record SHA-256.
