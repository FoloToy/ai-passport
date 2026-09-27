<p align="right">
  <a href="AGENTS.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Repository Guidelines for AI Agents

This file is the only mandatory entry point for AI-assisted work in this repository. Read task-specific documents from the routing table below; do not load every README by default.

## Required AI skills

The following five skills are required for AI-assisted development in this repository:
`passport-develop`, `passport-setup`, `passport-build`, `passport-device-test`, and
`passport-debug`. Their maintained sources are under `skills/`.

Before starting development, the AI must check that all five are installed and
available in its current environment. If any are missing, the AI must choose an
appropriate installation method for its tool and environment, perform the
installation, and verify availability itself. Do not wait for the user to request
installation or make the user choose the method or run installation commands.
No particular script, installation directory, or AI client is mandatory.

Respect the environment's approval requirements and preserve existing skills
and user configuration. If permissions, conflicts, or platform limitations prevent
installation, explain the blocker and request only the necessary user action;
do not claim installation succeeded. Having all five available does not mean
loading or invoking all five for every task: use only the matching skills, and
retain the separate authorization requirements for flashing, Git writes, and publishing.

## Project and safety baseline

- Target: ESP32-C3, 8 MB Flash, no PSRAM, ESP-IDF 5.5.3.
- Keep the repository's default partition table minimal: NVS, PHY data, and
  one factory application spanning the rest of the 8 MB Flash. User firmware
  may deliberately change this layout; validate the resulting table and do not
  turn product-specific partitions into mandatory template contracts.
- Preserve existing user changes. Start with `git status --short --branch`; never overwrite or clean unrelated files.
- Flashing new firmware does not require backing up the firmware already on the device; do not make a Flash readback a prerequisite. This does not guarantee preservation of user data or authorize a full-chip erase. Follow the [flashing and data policy](docs/development/engineering/firmware-layout.md#flashing-and-stored-data).
- Ask first: resolve ambiguity with the user before writing code, not after. Ask
  when a requirement, page flow, control mapping, or data behavior could be read
  more than one way; when the change adds wiring, affects electrical safety, or
  revises the board; when it fixes an irreversible data format; and before
  deleting or overwriting existing work. Batch related questions into one prompt
  and offer a recommended option rather than an open question. Use
  `AskUserQuestion` in Claude Code and the closest structured-question equivalent
  in other clients. Otherwise, choose the conservative default and list every
  assumption in the delivery.
- Hardware facts follow this priority: product specifications and measured results → `components/bsp/include/bsp_pins.h` → BSP headers and implementation → hardware guide → README/demo code. If a task requires a hardware detail not defined by these sources, ask the user instead of guessing.
- Reusable board logic belongs in `components/bsp`; pages, state machines, animations, and application tasks belong in `main`.
- Derivative applications must redesign and implement their own UI. Reusing the current demo test menu, screens, or visual shell is prohibited; renaming, recoloring, or adding a feature to that shell does not count as a redesign. BSP APIs and non-UI logic remain reusable. See the [mandatory UI redesign rule](docs/development/ai-guide.md#mandatory-ui-redesign-for-derivative-applications).
- LVGL is not thread-safe. Code outside the LVGL task must hold `bsp_lvgl_lock()` while accessing LVGL objects.
- Before adding Chinese UI text, follow the [font checklist](docs/development/engineering/coding-conventions.md#chinese-fonts-and-missing-glyphs). The default Montserrat fonts have no Chinese glyphs; UTF-8 and a successful build do not prove display support. Verify glyph coverage, widget font selection, and on-device rendering.
- Button callbacks must stay non-blocking. Audio, storage, networking, and other slow operations belong in worker tasks.
- A demo must stop every task, timer, callback, and event handler that can access its UI before deleting the screen.
- Keep testable state machines, protocols, timing, and layout calculations independent from ESP-IDF/LVGL and cover them with host tests.
- Never commit credentials, device QR secrets, private keys, personal data, or unsanitized logs.
- Every maintained Markdown document uses English at its default `.md` path and Simplified Chinese in a paired `.zh_CN.md` file. Keep both versions aligned and retain reciprocal language links.

## Task-specific context routing

| Task | Read before editing |
| --- | --- |
| Any code change | `docs/development/ai-guide.md`, relevant headers and neighboring implementation |
| Application workflow or core skill setup | `skills/README.md`; ensure the five required skills are available, then use only the matching skill |
| Environment bootstrap or missing toolchain | `docs/development/engineering/environment-setup.md` |
| BSP, pins, buses, display, audio, battery | `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`, `components/bsp/include/bsp_pins.h` |
| Demo or menu | `main/demo.h`, `main/main.c`, the nearest `main/demo_*.c` implementation |
| Chinese UI text or fonts | `docs/development/engineering/lvgl-chinese-fonts.md`, the application's font assets, configuration, and widget styles |
| Wi-Fi connection or Bluetooth provisioning | `docs/development/engineering/wifi-provisioning.md`, the referenced `demo/blufi-provisioning` implementation |
| Build, test, dependencies, partitions | `docs/development/engineering/build-and-test.md`, `docs/development/engineering/firmware-layout.md`, `sdkconfig.defaults`, `partitions.csv` |
| CI or release | the matching file in `docs/development/ci/CI-*.md` and `.github/workflows/` |
| Project completion | `docs/development/release/project-completion.md` (then the `issue-suggestions` or `experience-pr` skill) |
| Documentation | `docs/contribution/doc-conventions.md`, `docs/README.md` |
| Commit or PR | `docs/contribution/commit-and-pr.md` |

Use `docs/README.md` for the product overview and the documentation index. For the detailed AI development workflow — context setup, source-of-truth priority, application/BSP boundary, runtime invariants, material placement, and delivery format — read `docs/development/ai-guide.md`. Fork-specific workflow is in `docs/fork-guide.md` and is not required for ordinary upstream development.

## Required validation and delivery

Run the smallest relevant check while iterating, then run the complete gate before delivery:

```bash
./tools/validate.sh --static    # repository checks + host tests
./tools/validate.sh --firmware  # ESP-IDF build + merged-image verification
./tools/validate.sh             # complete gate
```

The complete gate requires an activated ESP-IDF 5.5.3 environment. Do not describe a successful build as hardware validation. Final delivery must report these fields separately:

```text
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: remaining board, instrument, or user checks
```

After each complete implementation of a user-requested firmware change,
proactively ask whether to flash it to the device for testing; do not wait for
a release request. If no device is detected, ask the user to turn it on and
connect it to a computer USB port with a data-capable cable. Follow the
[on-device testing handoff](docs/development/ai-guide.md#offer-on-device-testing)
and obtain approval before flashing; detection alone is not consent.

Create commits within a change the user has asked for, one logical unit at a time,
without asking before each commit; push only when the user requests it or the
active workflow explicitly requires it. Follow the [commit and PR
conventions](docs/contribution/commit-and-pr.md#commits) for unit granularity and
message content. Ordinary feature, application, and documentation pull requests must not edit `docs/CHANGELOG.md` or `docs/CHANGELOG.zh_CN.md`; describe user-visible behavior, compatibility, and release-flow impact in the pull-request body and authoritative documentation instead. During release preparation, the release maintainer aggregates merged user-visible changes into both changelog files before creating the tag.

Community guidance is in `.github/CONTRIBUTING.md`, `.github/CODE_OF_CONDUCT.md`, `.github/SECURITY.md`, and `.github/SUPPORT.md`.
