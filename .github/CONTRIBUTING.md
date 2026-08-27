<p align="right">
  <a href="CONTRIBUTING.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Contributing

Thank you for contributing to FoloToy AI Passport — code, documentation,
firmware, and feedback. This repository is the development baseline for
open-source wearable AI hardware designed for AI agents. It is often forked for
second development; the fork conventions are in
[`docs/fork-guide.md`](../docs/fork-guide.md).

## Before you start

- Read [`AGENTS.md`](../AGENTS.md): it is the authoritative entry and index for the
  rules an AI agent should follow. It is not a replacement for this guide.
- Read [`README.md`](../docs/README.md) for the hardware capability contract,
  and the [AI Hardware Development Guide](../docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md)
  for the complete hardware context.
- Follow [`CODE_OF_CONDUCT.md`](CODE_OF_CONDUCT.md) when participating in the
  community. For ordinary usage questions, see [`SUPPORT.md`](SUPPORT.md).
- Do not commit credentials, tokens, authorization files, or personal data.
- The repository's `main` branch stays in sync with the upstream baseline; fork
  users develop feature work in `feature/*` branches (see `docs/fork-guide.md`).

## Development and verification

Use ESP-IDF 5.5.3. For a clean-machine setup, follow the
[environment bootstrap](../docs/development/environment-setup.md).

Prefer the repository firmware gate for builds and flash its verified merged
image at `0x0`. The direct IDF commands below are for incremental development.

```bash
source <path-to-esp-idf-v5.5.3>/export.sh
idf.py --version             # must report ESP-IDF v5.5.3
./tools/validate.sh --firmware # preferred merged-image build
idf.py set-target esp32c3     # Configure the target chip (fresh checkout / after target change)
idf.py build                  # Optional incremental application build
idf.py flash monitor          # Optional incremental application flash
idf.py fullclean              # Clear stale build state (never for user source changes)
```

The current baseline includes a pure-logic test that runs without hardware:

```bash
cc -std=c11 -Wall -Wextra -Werror -Imain \
  tests/test_ui_pixel_math.c main/ui_pixel_math.c \
  -o /tmp/test_ui_pixel_math
/tmp/test_ui_pixel_math
```

The repository provides one validation entry point for local development and CI:

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware  # requires an activated ESP-IDF 5.5.3 environment
./tools/validate.sh             # complete gate
```

Follow the verification requirements in
[`docs/development/build-and-test.md`](../docs/development/build-and-test.md):
a clean `idf.py build` is the minimum automated check, not hardware validation.
Record build results and on-device results separately; never present a successful
build as successful hardware validation.

## Opening a pull request

1. Create a short-lived `feature/*` branch from `main` and keep each pull request
   focused on one clear problem.
2. Use `<type>(<scope>): <short description>` for the pull request title, for
   example `feat(bsp): ...`, `docs: ...`. Available types are defined in
   [`docs/contribution/commit-and-pr.md`](../docs/contribution/commit-and-pr.md).
3. Review the complete diff and confirm that it contains no credentials,
   unrelated generated files, or unintended changes.
4. Follow the PR requirements in `docs/contribution/commit-and-pr.md`: state the
   hardware/revision tested, summarize behavior changes, list build and on-device
   results, link related issues, and record observed on-device results for pin,
   display-rotation, codec-clock, ADC, or DMA changes.
5. Wait for CI and review; do not push directly to `main` unless you are a
   maintainer handling an explicit exception.

Small documentation fixes are welcome as pull requests. For larger changes to
hardware, architecture, or user data, please open an issue first to discuss
scope and compatibility.

## Licensing of contributions

This repository is licensed under [MIT](../LICENSE). By contributing, you agree that
your contribution is submitted under the MIT license terms of the repository.

## Security issues

Do not disclose vulnerabilities, credentials, or exploitable details in public
issues, pull requests, or discussions. Follow the private reporting process in
[`SECURITY.md`](SECURITY.md).
