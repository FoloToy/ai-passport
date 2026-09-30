<p align="right">
  <a href="asset-pack-name-contract.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# A Packed Asset Name Is a Contract Between the Packer and the Firmware

Collected after releasing **Asunabi**, a visual-novel reader that reads its artwork
and chapter scripts out of a read-only flash partition instead of embedding them in
the application. The partition is built at compile time from a source tree that the
fork keeps outside the firmware image.

## A lookup that misses is a blank screen, not an error

The reader asks the pack for a few names by literal, for example the title
backdrop `bg/index_bg.png`. The packer stored that same bitmap under the name
`index_bg.png` — the prefix was dropped when the image was collected — so the
lookup missed. Nothing failed: the build was green, the bitmap was in the pack
(77,824 bytes, verified by the pack inspector), and the only evidence was one
warning line in the boot log. The screen fell through to its fallback, which was
black, and the release booted into a black title screen with the artwork sitting
unused a few hundred kilobytes away in the same image.

Treat every name the firmware looks up as a contract with the packer, and keep the
failure loud: a missing lookup should at least be a compile-time or test-time
error, not a silent fallback that still boots.

## A placeholder path can hide the mismatch

The packer emits a placeholder pack when the artwork tree is absent, so a clone
without the third-party material still configures, builds and boots. That path had
the prefix right, because its names are written by hand as solid-fill entries. The
mismatch therefore only appeared once real artwork became the default build, which
is exactly the configuration a release ships.

If a pipeline has two sources for the same output — real material and a placeholder
— compare their names against the firmware, not just against each other.

## One declaration, one test

The fix was to make the packer's UI-image list hold the **pack names** (which are
also the paths below the source root), so there is a single place where a name
exists, and then to assert it against the firmware's lookup literals:

- Walk the firmware sources for the lookup calls, for example
  `gal_assets_find(..., "name")`, and collect the string literals.
- Compare them with the packer's declaration: a lookup the packer never packs, and
  a packed UI image nothing looks up, are both failures.
- Keep the test free of the image library: import the packer as a module (it
  imports its image library lazily) and read the declaration, so the check runs in
  the same host-test suite as the rest of the logic.
- Wire it into the repository validation script, which is what makes CI catch the
  regression rather than a device test.

## Budget the partition from the source

3.0 MiB of source art (89 PNG files) and 30 JSON chapter scripts pack into
3.55 MiB: about 3.19 MiB of image payload plus the scripts and their text pools.
The partition is 4 MiB, and the packer takes a maximum size and **fails the build**
instead of emitting a truncated image, so the two numbers cannot drift apart
silently. Artwork is scaled to the panel at pack time rather than stored at source
resolution, and character sprites are cropped to the region that can reach the
panel before their alpha bounding box is taken.

## If the material lives in the repository, CI needs the image library

Once the artwork is tracked, every build packs real material, and packing needs an
image library (Pillow here) in the interpreter the IDF build uses. A workflow that
installs it keeps the tag-triggered release honest; without that step the same
build goes red at the packer, which is at least a loud failure — the opposite of
the problem at the top of this entry.

## Related

- [What Flashing a Merged Image Does to Stored Data](merged-image-flashing-and-stored-data.md)
- [Size Static Buffers from the Panel, and Verify the Release Artifact](release-artifact-verification.md)
