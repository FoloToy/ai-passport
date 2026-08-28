---
name: issue-suggestions
description: After a firmware release, gather the releasing developer's own improvement points and file them as issues against the upstream FoloToy ai-passport project.
---

<p align="right">
  <a href="SKILL.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Collect Suggestions and File Issues

This skill turns the releasing developer's own improvement points into
actionable GitHub issues against the upstream `FoloToy/ai-passport` project. It
runs only after a firmware release has been published (see
`docs/development/publish-to-community.md` for publishing itself) and only when
the developer explicitly asks to collect suggestions.

## Safety and consent gate (run first)

Do not proceed with any collection, drafting, or submission until every gate
below is satisfied. These gates are the highest-priority constraints.

1. **Confirm consent up front.** This work touches project-private content.
   Ask the developer to confirm they agree to start the post-release follow-up
   before collecting anything. If they decline, stop immediately.
2. **Check GitHub access.** Confirm a GitHub channel is available before
   submitting. Try the configured channels in order and use the first one that
   works: GitHub MCP (when present and authenticated), a GitHub skill (for
   example the installed `cindy-github` capability), then the `gh` CLI
   (`gh auth status`). If none is available, generate the complete issue content
   for the developer to paste manually and stop; never submit on their behalf.
3. **Never submit before review.** Draft everything first, show it to the
   developer, and wait for explicit approval. Do not create, edit, or close any
   issue until the developer has reviewed and authorized it.
4. **No credentials or private data.** Never include credentials, device QR
   secrets, private device links, personal data, or unsanitized logs.

## Collect the developer's improvement points

Gather the improvement points the releasing developer encountered while
developing or shipping this release that would benefit the upstream project:

- Pain points, awkward workarounds, or gaps in the BSP, build, or tooling.
- Constraints or behaviors that should be documented or relaxed upstream.
- Ideas for making the next release smoother.

Do **not** collect external requests from the upstream issue tracker as a source;
this skill captures the developer's own improvement points. If an upstream issue
already covers a point, see the matching step below.

## Screen and categorize

- **Deduplicate**: fold repeated or overlapping points into one.
- **Drop** invalid, off-topic, or already-resolved points.
- **Merge** related points into a single coherent suggestion.
- **Categorize** by affected area, matching the issue template options:
  - Firmware or demo
  - BSP or hardware interface
  - Documentation or tooling
  - Hardware revision
  - Not sure

## Check for an existing issue

For each surviving point, search issues and PRs in the upstream project for a
matching entry:

- If a matching issue already exists, do **not** create a duplicate. Append a
  comment or note the relationship instead.
- If no matching issue exists, draft a feature request issue using the fields
  of the upstream `.github/ISSUE_TEMPLATE/feature_request.yml` (problem or use
  case, proposed behavior, affected area, alternatives, additional context,
  safety check).

## Draft, review, and submit

1. Draft the issue content (English title and body, keeping field labels as the
   template defines), sanitized of credentials and private data.
2. Present the draft to the developer and wait for explicit authorization.
3. Only after approval, submit through the first available GitHub channel
   (GitHub MCP, a GitHub skill, or `gh issue create --repo FoloToy/ai-passport`)
   and then read the created issue back to confirm.

## What this skill does not do

- It does not publish firmware or run the publisher workflow.
- It does not open code pull requests or modify repository code.
- It does not auto-submit anything without developer review and consent.

## Related documents

- Project completion overview: `docs/development/project-completion.md`
- Filing issues: `docs/development/file-issues.md`
- Firmware publishing: `docs/development/publish-to-community.md`
- Issue template: `.github/ISSUE_TEMPLATE/feature_request.yml`
- Contribution and commit rules: `docs/contribution/commit-and-pr.md`
