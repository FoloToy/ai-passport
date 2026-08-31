<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Development Guidelines

This directory contains AI Passport engineering rules and reusable workflows, grouped by purpose: the AI-assisted development workflow (`ai-guide.md`), engineering conventions (`engineering/`), CI documents (`ci/`), and the release/completion flow (`release/`). Rules should identify their trigger, required action, prohibited action, validation, and exceptions. Hardware facts belong in `docs/hardware-design/`; automatable requirements must also be enforced by tooling or CI.

## AI workflow

- [ai-guide.md](ai-guide.md): AI-assisted development workflow.

## Engineering

- [environment-setup.md](engineering/environment-setup.md): clean-machine bootstrap for AI agents, including international and mainland China download routes.
- [build-and-test.md](engineering/build-and-test.md): ESP-IDF build and validation.
- [ble-recovery-compatibility.md](engineering/ble-recovery-compatibility.md): mandatory mini-program BLE install artifact, partition, and bootloader contract.
- [coding-conventions.md](engineering/coding-conventions.md): source-code and resource conventions.

## CI

- [CI-validation.md](ci/CI-validation.md): pull-request and main-branch checks.
- [CI-build-and-release.md](ci/CI-build-and-release.md): tagged firmware builds and releases.
- [CI-sync-main.md](ci/CI-sync-main.md): upstream synchronization for forks.

## Release

- [publish-to-community.md](release/publish-to-community.md): publishing firmware to the AI Passport community market.
- [project-completion.md](release/project-completion.md): project completion flow — a menu of optional closing actions.
- [file-issues.md](release/file-issues.md): filing a suggestion as an upstream GitHub issue.
