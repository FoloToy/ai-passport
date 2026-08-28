<p align="right">
  <a href="AGENTS.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Repository Guidelines for AI Agents

This file is the only mandatory entry point for AI-assisted work in this repository. Read task-specific documents from the routing table below; do not load every README by default.

## Project and safety baseline

- Target: ESP32-C3, 8 MB Flash, no PSRAM, ESP-IDF 5.5.3.
- Preserve existing user changes. Start with `git status --short --branch`; never overwrite or clean unrelated files.
- Hardware facts follow this priority: product specifications and measured results → `components/bsp/include/bsp_pins.h` → BSP headers and implementation → hardware guide → README/demo code. If a task requires a hardware detail not defined by these sources, ask the user instead of guessing.
- Reusable board logic belongs in `components/bsp`; pages, state machines, animations, and application tasks belong in `main`.
- LVGL is not thread-safe. Code outside the LVGL task must hold `bsp_lvgl_lock()` while accessing LVGL objects.
- Button callbacks must stay non-blocking. Audio, storage, networking, and other slow operations belong in worker tasks.
- A demo must stop every task, timer, callback, and event handler that can access its UI before deleting the screen.
- State machines, protocols, timing rules, persistence formats, and layout calculations that do not require hardware must be independent from ESP-IDF/LVGL and covered by `tests/test_*.c`.
- Never commit credentials, device QR secrets, private keys, personal data, or unsanitized logs.
- Every maintained Markdown document uses English at its default `.md` path and Simplified Chinese in a paired `.zh_CN.md` file. Keep both versions aligned and retain reciprocal language links.

## Task-specific context routing

| Task | Read before editing |
| --- | --- |
| Any code change | `docs/development/agent-guide.md`, each header that declares a modified symbol, and the implementation and callers changed by the task |
| Environment bootstrap or missing toolchain | `docs/development/environment-setup.md` |
| BSP, pins, buses, display, audio, battery | `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`, `components/bsp/include/bsp_pins.h` |
| Demo or menu | `main/demo.h`, `main/main.c`, the nearest `main/demo_*.c` implementation |
| Build, test, dependencies, partitions | `docs/development/build-and-test.md`, `sdkconfig.defaults`, `partitions.csv` |
| CI or release | `docs/development/build-and-test.md` and `.github/workflows/` |
| Documentation | `docs/contribution/doc-conventions.md`, `docs/INDEX.md` |
| Commit or PR | `docs/contribution/commit-and-pr.md` |

Use `docs/README.md` for the product overview and `docs/INDEX.md` when a task needs additional documentation.

## Required validation and delivery

During iteration, documentation and repository-policy changes run `python3 tools/check_repo.py`; host-test or pure-logic changes run `./tools/run-host-tests.sh`; workflow changes run `./tools/validate.sh --static`; firmware changes run `./tools/validate.sh --firmware`. Every delivery runs the complete gate:

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

Create commits and push only when the user requests them or the active workflow explicitly requires them. Ordinary feature branches document application behavior in their own README and do not append to the upstream changelog. The release maintainer updates `docs/CHANGELOG.md` for released baseline behavior and compatibility.

Community guidance is in `.github/CONTRIBUTING.md`, `.github/CODE_OF_CONDUCT.md`, `.github/SECURITY.md`, and `.github/SUPPORT.md`.
