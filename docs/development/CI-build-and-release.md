<p align="right">
  <a href="CI-build-and-release.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Automated Build and Release

`.github/workflows/build-firmware.yml` builds and publishes firmware for tags and supports manual dispatch. Ordinary branch pushes do not trigger it. Keep this page synchronized with the workflow.

The build job restores ccache, runs `./tools/validate.sh --firmware` with ESP-IDF 5.5.3 for ESP32-C3, verifies the bootloader at `0x0`, partition table at `0x8000`, application at `0x10000`, and 8 MB Flash arguments, then uploads `FoloToy-AI-Passport-full.bin`. A separate least-privilege release job publishes that artifact only for a tag.

All Actions are pinned to full commit SHAs. The build job has `contents: read`; only the tag release job receives `contents: write`.

## Browser flashing

Open `https://ai-passport.folotoy.cn/tools/web-flasher/`, connect the USB JTAG/serial device, select the release's merged `FoloToy-AI-Passport-full.bin`, choose a baud rate such as 460800, and write it from `0x0`. The browser performs local writing and verification; it does not upload the firmware file.

For board and flashing details, see [the hardware development guide](../hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md).

## Release title

When this repository publishes firmware for several different applications from
the same source tree, a bare version number does not tell a user which app a
release is for. Give each tag a name that carries the version and the app, and
make sure the release title shows both.

- **Tag naming convention**: name tags as `v<version>-<app-name>` in
  lowercase-kebab-case, e.g. `v0.1.0-voice-keychain`, `v1.0.0-pocket-pomodoro`.
  The `<app-name>` is the application this release builds (see the
  `plays/<username>/<app-name>/` archive naming). A tag that only says a version
  is ambiguous when several apps share the tree.
- **After the release is published, confirm the release title.** The workflow
  sets the title to the tag name, so a correctly-named tag already reads
  `v0.1.0-voice-keychain`. If the tag did not include the app, or the title is
  not obvious at a glance, edit the release (GitHub: `Edit release`; GitLab:
  edit the tag) so the title is `<version> <app-name>`, e.g. `v0.1.0 Voice
  Keychain`. One quick scan of the release list should distinguish which app a
  release is for.
- **Keep title and tag consistent.** Use `<version>-<app-name>` so the app name
  is visible in both the tag list and the release list. Do not rely on a
  human-readable body alone to carry the app name.

## Release notes

A tag-triggered release succeeds only when the merged firmware and its release
notes travel together. After the release is published, write release notes that
explain the build to a user who may not have read the repository. Cover three
things:

- **What's new**: the features, behaviors, or fixes this release adds or
  changes compared with the previous one. Keep it user-facing, not a commit log.
- **How to build**: how to produce and verify the merged firmware
  (`./tools/validate.sh --firmware` or `idf.py build`), and the artifact file to
  flash (`FoloToy-AI-Passport-full.bin` flashed from `0x0`).
- **How to use**: how to flash the build (the browser flasher above) and the key
  interactions or hardware requirements of the release.

Write the release notes in English (and a Simplified Chinese version where the
project is bilingual) and link them from the GitHub/GitLab release. Keep them
consistent with `docs/CHANGELOG.md` for user-visible behavior.

## Sync the fork's root README

A single fork repository may host several different development projects. After
publishing a release, in addition to setting the release title and writing the
release notes, also update the root `README.md` on your fork's `main` branch so
the released application is discoverable and reachable from the repository's own
landing page. The fork root README is the **overview / catalog of the projects
this fork hosts**: it introduces each project's content (what it does, how it is
used, and the key behavior or notes), and links to that project's own README,
source, release, and usage notes. It is not just a list of branch links.

This applies to the README a fork owner maintains for their own fork (the root
`README.md` / `README.zh_CN.md` pair reserved for the fork owner per
[`docs/fork-guide.md`](../fork-guide.md)); it is fork-owned content and is not
part of the upstream proposal.

When releasing an application:

- Add or update the fork's root README so the released application is introduced
  among the fork's projects: give a short description of what it does and how to
  use it, and link to the application's own README/source and how to get it (the
  source address, the release, and any key usage or flashing notes). Keep it
  aligned with the release title and notes.
- If the fork has no root README yet, create the bilingual `README.md` /
  `README.zh_CN.md` pair as the project catalog for fork `main` so every project
  (including the new release) is presentable from the repository page.
- Do **not** push this README upstream. The root README path is reserved for
  the fork owner; upstream's overview is `docs/README.md`.

See [`../fork-guide.md`](../fork-guide.md) for the fork root-README convention.

## Related documents

- Firmware publishing to the community: [publish-to-community.md](publish-to-community.md)
- Post-release follow-up: [after-release.md](after-release.md)

