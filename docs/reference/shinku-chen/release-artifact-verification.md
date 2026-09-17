<p align="right">
  <a href="release-artifact-verification.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Size Static Buffers from the Panel, and Verify the Release Artifact

Collected after releasing **Connect Four**, where the first published firmware
booted with only 8 KB of free heap and had to be rebuilt before the community
submission.

## A convenient constant can cost 50 KB

The release adds a serial screenshot command (`FAP_SCREENSHOT_V1`) whose frame
buffer must be reserved statically: with LVGL, the codec and the DMA buffers
already running, the heap cannot produce a 150 KB contiguous block. The buffer was
declared as "the largest of the two orientations" — 320 × 320 pixels, 204,800
bytes — while one frame is exactly the panel's pixel count: 240 × 320 =
**153,600 bytes, the same in portrait and landscape**.

The extra 51 KB does not show up in a flash-size report, and it is not a build
error. On the device the free heap fell from 227 KB to 8.3 KB with a largest block
of 7.7 KB, which happens to still boot and play, so nothing fails loudly. Derive
such buffers from the panel geometry (`BSP_LCD_W * BSP_LCD_H * 2`) instead of from
a round number, and give the feature a compile-time switch so a build can hand the
RAM back.

## Flash the artifact you published, and read the boot log

The workflow already says to verify a release on hardware; the practical version of
that is: download the **released** merged image, flash it from offset 0, and read
the startup log for the numbers that matter — logical resolution, task readiness,
and free heap. That check caught the buffer mistake in minutes.

Reproducing the issue locally also required flashing the same artifact rather than
a local build, because the two differ: the CI build produced different bytes for
the same source. A fix verified only in a local build would not have proven
anything about what users download.

When a just-published release turns out to be wrong and nothing has consumed it yet,
replacing it — deleting the release and the tag, moving the tag to the fixed
commit, and letting the tag-triggered build republish — is cleaner than shipping a
follow-up version that only fixes a mistake nobody could see.

## The screenshot command is also the cover pipeline

With that command in place, a release cover can show the device's real frame
instead of a redrawn mock-up: capture the frame over serial, composite it into the
brand shell, and place the result in a scene. The cover is then publish metadata —
recorded by file name and format — and does not need to be committed.
