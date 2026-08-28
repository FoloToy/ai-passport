<p align="right">
  <a href="repository-optimization-plan.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Minimal Repository Implementation Record

## Accepted objective

The repository is the smallest self-contained baseline that supports this exact AI workflow:

```text
read AGENTS.md -> establish ESP-IDF 5.5.3 -> implement the requirement
-> run repository and host checks -> build ESP32-C3 firmware
-> verify and output build/FoloToy-AI-Passport-full.bin
```

The workflow uses the current checkout only. Remote demo branches, community archives, websites, private files, and developer-specific shell configuration are never required inputs.

## Retained baseline

- `AGENTS.md` is the only mandatory entry point; `CLAUDE.md` only redirects compatible tools to it.
- `components/bsp/` owns the production board implementation and public APIs.
- `main/` retains exactly seven runnable pages: display, buttons, audio, battery, Wi-Fi scan, Bluetooth LE advertising, and light/deep sleep.
- `tests/` contains hardware-independent logic tests; `tools/run-host-tests.sh` is their execution entry point.
- `tools/validate.sh` is the single local and CI validation entry point.
- ESP-IDF inputs are `CMakeLists.txt`, `sdkconfig.defaults`, `partitions.csv`, `dependencies.lock`, and `.clangd`.
- Maintained documentation is limited to product capability, AI workflow, environment setup, build/test/output, coding and contribution rules, product specifications, board facts, acceptance, and released history.

## Removed content

The repository owner authorized direct removal without migration. Git history remains the recovery source.

| Removed area | Reason |
| --- | --- |
| `plays/` | Application archive is outside firmware development and is not executable baseline code |
| `docs/experiences/` | Narrative experience records duplicate facts that belong in authoritative engineering documents |
| `skills/` | Issue, experience, and archive automation runs after firmware development |
| `docs/assets/brand/` and `docs/brand-and-product.*` | Marketing assets and live website/catalog information are not firmware inputs |
| Community publishing, post-release, and issue-filing guides | These define a second lifecycle after firmware output |
| Fork guide and automatic fork-sync workflow | Downstream fleet synchronization is not part of building this checkout |
| Empty root asset and software-design scaffolding | Empty tracked structure has no runnable or documentary responsibility |
| Standalone CI guides | Their unique instructions are consolidated into `build-and-test.md` |

## Hardware decisions recorded on 2026-08-28

- The pin and device mapping applies to every shipped standard-production unit.
- The battery is 520 mAh and CW2017 is fitted on every production unit.
- I2C SDA GPIO10 and SCL GPIO7 each have a 2.2 kΩ external pull-up to 3.3 V.
- The production-measured button windows remain `[0,150)`, `[150,447)`, and `[447,1900)` mV.
- Any GPIO0 function button is an active-low wake source for light sleep and deep sleep.
- The external power circuit owns the power button: a continuous 0.5-second hold starts the device and a continuous 2-second hold shuts it down; firmware cannot read it.
- The amplifier has no MCU enable pin, is battery-powered, and uses 4.2 V as the fixed full-charge `esp_codec_dev` gain-calibration value.
- Device QR/recovery behavior is not an engineering hardware interface and is removed from repository specifications.

## CI contract

The repository keeps exactly three workflows:

1. `static-checks.yml` runs repository checks, host tests, and actionlint on every pull request and every `main` push.
2. `firmware-checks.yml` builds ESP-IDF 5.5.3 only when firmware, configuration, component, partition, or firmware-validation paths change; manual dispatch is also enabled. It retains the verified merged image for seven days.
3. `build-firmware.yml` builds the same verified image on manual dispatch and version tags; only a tag run creates a GitHub Release.

All actions use full commit SHAs. All build paths call `tools/validate.sh`; YAML does not duplicate firmware assembly or verification logic.

## Remote work disposition

The integration pull request supersedes PRs #21, #24, #26, and #27. PR #24's durable GPIO-wake, direct-display, and LVGL object-safety facts are represented in authoritative code and documents; the experience archive itself is removed. The integration PR contains `Closes #22` and `Closes #23`, so those issues close only when the implementation reaches `main`.

## Validation state

The implementation is accepted only when all of the following are true:

- `./tools/validate.sh --static` passes repository checks, every host test, and workflow lint.
- `./tools/validate.sh --firmware` builds with ESP-IDF 5.5.3 and byte-verifies the merged image at the partition offsets.
- `./tools/validate.sh` passes the complete gate.
- `git diff --check` reports no whitespace errors.
- A clean checkout can run the documented commands without any remote demo branch.
- Device acceptance tests every page and separately tests UP, DOWN, and OK wake for both light sleep and deep sleep.

Device tests remain `NOT RUN` until the user connects a standard-production board. International and mainland-China clean-machine bootstrap remain `NOT RUN` until each route is executed on a machine without an existing ESP-IDF installation. These states are explicit test results, not implementation assumptions.
