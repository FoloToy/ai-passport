# Pixel Buddy UI Design

## Goal

Replace the deformed character-art pets with real pixel sprites and refine the 240 × 320
interface so overlays, text, and pets never compete for the same pixels. Preserve the
existing BLE protocol, security behavior, state machine, controls, and 18-species choice.

## Layout

Use the approved high-information-density layout with four fixed vertical regions:

- Status bar: 26 px, always visible.
- Pet stage: 132 px, with the species name above the sprite.
- Information area: 138 px for page-specific content.
- Action bar: 24 px, always reserved and never used by scrolling content.

Each region has an explicit clipping rectangle. The pet stage centers the sprite from its
actual non-transparent bounds, not a species-wide assumed origin. Pet sprites use a common
64 × 64 design grid and may occupy a smaller bounding box when their silhouette requires it.

## Layering

The base page is painted first, followed by exactly one top-level overlay. Confirmation and
pairing overlays take priority over approval, which takes priority over the menu. An overlay
first dims its entire base-page coverage, then paints an opaque panel. Base-page text inside
the panel rectangle must not remain visible.

Approval uses the lower half of the screen, retains the status bar and a recognizable pet
silhouette, and reserves a fixed footer for OK/DOWN actions. Confirmation and pairing may use
a taller modal. Overlay body text is clipped independently from its header and footer.

## Typography

Use the 8 px pixel font for status, body, values, and actions. Use the 16 px pixel font only
for page and modal titles. Text wraps at word boundaries; a word wider than the content box
is clipped with an ellipsis instead of overflowing. Labels align left and values align right.
Species names and user-provided name/owner/status fields are ellipsized to their region.

Approval shows the complete bounded tool name and up to five wrapped hint lines. Its action
footer never scrolls. Scrolling content is clipped above the fixed action bar.

## Pixel Sprites

Implement all 18 existing species as programmatic pixel sprites rather than ASCII or raster
assets. A shared drawing API provides clipped rectangles, pixels, lines, and small effects on
the existing I4 framebuffer. Each species supplies layered geometry for its silhouette,
body color, face, limbs, and identifying decoration.

Each species supports seven semantic states:

- sleep: closed eyes and slow breathing;
- idle: blinking and subtle body motion;
- busy: faster working motion;
- attention: alert pose;
- celebrate: bounce and confetti;
- dizzy: displaced eyes or orbiting marks;
- heart: affectionate pose and heart effect.

Animations use a small number of deterministic keyframes derived from the existing UI tick.
No heap allocation, image decoding, GIFs, or PSRAM is required. Home uses the largest sprite
that fits its stage; Pet uses the compact stage size; approval may render a reduced/dimmed
sprite behind the opaque lower panel.

## Architecture

Keep I4 pixel primitives independent from LVGL and host-testable. Put species geometry and
animation selection behind a bounded sprite renderer API. `buddy_ui.c` owns layout, clipping,
layer order, and text composition; it consumes only immutable state snapshots. BLE callbacks
and state transitions remain unchanged.

The existing imported ASCII renderers are removed from the production firmware after the
new renderer covers every species/state. The final UI and firmware build must not invoke or
link them.

## Validation

Host tests cover framebuffer bounds, clipping, word wrapping/ellipsis, all 18 species, all
seven states, non-empty sprite bounds, and stable dimensions across animation frames. Layout
tests assert that status/action bars and modal header/footer rectangles do not overlap body
text or sprites.

Run the complete host suite, ESP-IDF build, size report, and `git diff --check`. Hardware
acceptance must inspect every species and state on the 240 × 320 display, exercise long
name/status/approval strings, and confirm a watchdog-free connected soak. Hardware checks
remain NOT RUN until performed on a physical board.
