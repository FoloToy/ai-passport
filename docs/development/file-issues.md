<p align="right">
  <a href="file-issues.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Filing Issues

This page describes how to file a suggestion as a GitHub issue against the
upstream `FoloToy/ai-passport` project. It is the documentation counterpart to
the `issue-suggestions` skill, which drives the workflow; this page explains
the rules and conventions.

Issues are for user suggestions, feature requests, and usage feedback about the
firmware, BSP, hardware, or documentation. They are filed against the upstream
project, not the fork.

## Issue types and templates

The repository defines issue templates under `.github/ISSUE_TEMPLATE/`. Use the
one that matches the report:

- `feature_request.yml` for a new feature or improvement.
- `usage_question.yml` for help building, flashing, configuring, or using the
  device.

Fill the template fields completely. Feature requests use these fields:

- Problem or use case
- Proposed behavior and acceptance criteria
- Affected area (firmware or demo, BSP or hardware interface, documentation or
  tooling, hardware revision, not sure)
- Alternatives considered
- Additional context
- Safety check

Write the title and body in English for an upstream-facing issue; keep the
template's field labels as the template defines them (bilingual).

## Collect and screen

Gather the improvement points the releasing developer encountered while
developing or shipping this release, then screen them:

- Deduplicate overlapping suggestions into one.
- Drop invalid, off-topic, or already-resolved entries.
- Merge related items into a coherent feature request.
- Match against existing issues and PRs; do not create duplicates.

## Review and consent before filing

Filing an issue is a public, externally visible action and touches
project-related content. Follow these gates:

1. Confirm with the developer that they agree to start this follow-up.
2. Confirm a GitHub channel is available, trying in order GitHub MCP, a GitHub
   skill, then the `gh` CLI; if none is available, hand the draft to the
   developer to paste manually.
3. Draft the issue, present it to the developer, and wait for explicit approval.
4. Only after approval, create it through the first available GitHub channel
   (GitHub MCP, a GitHub skill, or
   `gh issue create --repo FoloToy/ai-passport`), then read it back to confirm.

## Safety

Never include credentials, device QR secrets, private device links, personal
data, or unsanitized logs. Security vulnerabilities must go through
`.github/SECURITY.md`, not a public issue.

## Related documents

- Project completion overview: [project-completion.md](project-completion.md)
- Issue skill: [`../../skills/issue-suggestions/SKILL.md`](../../skills/issue-suggestions/SKILL.md)
- Firmware publishing: [publish-to-community.md](publish-to-community.md)
