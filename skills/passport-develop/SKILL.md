---
name: passport-develop
description: Implement or extend a FoloToy AI Passport application from a user requirement, coordinating implementation, validation, and device-test handoff. Not for advice-only questions, log-only diagnosis, or publishing.
---

<p align="right"><a href="SKILL.zh_CN.md">简体中文</a> · <strong>English</strong></p>

# Develop an AI Passport Application

Use this workflow for an implementation request, not as permission to start a
different project. The user's explicit choices take precedence over this
skill's defaults. Resolve the target checkout with `git rev-parse --show-toplevel`;
all paths below are relative to that checkout, not the installed skill folder.
Read `AGENTS.md` and `docs/development/ai-guide.md`, then only the task's routed
documents. Start with `git status --short --branch`.

## Turn the request into a working increment

1. Establish the intended behavior and acceptance checks: screens, three-button
   actions, persistent data, networking/audio, and failure states as relevant.
   Ask only questions that materially affect the implementation; state reasonable
   defaults. Do not add Wi-Fi, cloud services, new wiring, or paid dependencies
2. [Strictly protect main branch, isolate development in feature branch]: Per
   `docs/fork-guide.md`, the `main` branch must remain a clean synchronization baseline
   with `upstream/main`; never develop or commit product-specific firmware directly on
   `main`. Verify the active branch (`git status -sb`) before editing; if on `main`,
   branch to `feature/*` first. Continue an existing application on its intended branch,
   preserving dirty work. Do not switch, stash, reset, or overwrite someone else's
   changes to obtain a clean starting point. If work overlaps or branch choice is
   ambiguous, resolve it with the user. Do not commit, push, or publish without authorization.
3. Consult relevant demo branches and `docs/reference/README.md`. Extract the
   needed patterns, not whole branches or their stale BSP/configuration. Apply
   the AI guide's mandatory UI redesign rule: derivative applications must have
   their own screens and interactions, never the current demo test menu, screens,
   or visual shell. Renaming or recoloring that shell is not a redesign. Reuse
   BSP APIs and isolated logic; do not rewrite drivers just to change the UI.
4. Implement the smallest complete behavior with tests for pure logic. Keep
   application state/tasks in `main` and reuse the BSP. Apply the existing LVGL
   locking, callback, teardown, RAM, and configurable-partition rules. Consult
   the Chinese-font or Wi-Fi provisioning guide only when the feature needs it.
5. Run focused tests while editing and the complete repository gate before
   delivery. Repair failures caused by the change; do not suppress checks or
   sweep unrelated failures into the task. Report environmental blockers with
   their evidence and the remaining actionable step.

## Route specialist work only when needed

If these skills are installed, use them for their stage. Otherwise read the
listed repository document and perform the same scoped workflow; never claim
to invoke a skill unavailable to the current agent.

| Situation | Skill | Repository fallback |
| --- | --- | --- |
| Missing or broken toolchain | `passport-setup` | `docs/development/engineering/environment-setup.md` |
| Validate and package firmware | `passport-build` | `docs/development/engineering/build-and-test.md` |
| User accepts on-device testing | `passport-device-test` | `docs/development/ai-guide.md`, on-device handoff |
| Runtime failure or crash | `passport-debug` | `docs/hardware-design/AI_HARDWARE_DEVELOPMENT_GUIDE.md`, troubleshooting |

## Handoff and continuation

Report implemented behavior, actual validation, remaining checks, and the next
step. Describe the redesigned pages and controls, and check that startup and
navigation no longer lead to the baseline test UI before calling the application
complete. For a multi-stage task, keep a concise checkpoint in the application's
existing project notes: accepted requirements, completed/pending work, exact
build identity, and device results. If new maintained Markdown is warranted,
use paired documents under `docs/`; never store secrets or raw private logs.
Do not create a progress-document framework for a small edit.

After a completed firmware implementation, proactively offer device testing
using `docs/development/ai-guide.md`. A build or connected USB device does not
authorize flashing. Stop for the user's decision if the remaining work requires
new authority; do not turn that pause into an automatic release.

Report `Build`, `Host tests`, `Device tests`, and `Unverified` separately.
Documentation-only work does not need unrelated firmware flashed to a device.
