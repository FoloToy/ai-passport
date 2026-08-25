<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Development Guidelines

This directory contains AI Passport engineering rules and reusable workflows. Rules should identify their trigger, required action, prohibited action, validation, and exceptions. Hardware facts belong in `docs/hardware-design/`; automatable requirements must also be enforced by tooling or CI.

## Documents

- [agent-guide.md](agent-guide.md): AI-assisted development workflow.
- [build-and-test.md](build-and-test.md): ESP-IDF build and validation.
- [coding-conventions.md](coding-conventions.md): source-code and resource conventions.
- [CI-validation.md](CI-validation.md): pull-request and main-branch checks.
- [CI-build-and-release.md](CI-build-and-release.md): tagged firmware builds and releases.
- [CI-sync-main.md](CI-sync-main.md): upstream synchronization for forks.
