<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Development Guidelines

This directory contains AI Passport engineering rules and reusable workflows. Rules should identify their trigger, required action, prohibited action, validation, and exceptions. Hardware facts belong in `docs/hardware-design/`; automatable requirements must also be enforced by tooling or CI.

## Documents

- [agent-guide.md](agent-guide.md): AI-assisted development workflow.
- [environment-setup.md](environment-setup.md): clean-machine bootstrap for AI agents, including international and mainland China download routes.
- [build-and-test.md](build-and-test.md): ESP-IDF build and validation.
- [ble-recovery-compatibility.md](ble-recovery-compatibility.md): mandatory
  mini-program BLE install artifact, partition, and bootloader contract.
- [coding-conventions.md](coding-conventions.md): source-code and resource conventions.
- [CI-validation.md](CI-validation.md): pull-request and main-branch checks.
- [CI-build-and-release.md](CI-build-and-release.md): tagged firmware builds and releases.
- [CI-sync-main.md](CI-sync-main.md): upstream synchronization for forks.
- [publish-to-community.md](publish-to-community.md): publishing firmware to the AI Passport community market.
- [project-completion.md](project-completion.md): project completion flow — a menu of optional closing actions.
- [file-issues.md](file-issues.md): filing a suggestion as an upstream GitHub issue.
- [experience-notes.md](experience-notes.md): index of development experience entries under `docs/experiences/`.
