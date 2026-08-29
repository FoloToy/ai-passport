<p align="right">
  <a href="publish-to-community.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Publish to the AI Passport Community

This document describes how to publish this project's firmware to the [AI Passport Community](https://ai-passport.folotoy.cn). This page is the human-facing entry point; it tells an AI assistant to install the publisher skill from the official bundle and run the workflow.

## When to use this

Use this when the project is being **released** — that is, when the code is finished, usable, and ready to ship. A release is not a commit: a commit saves progress during development, while a release happens only when development is complete and the firmware actually works. This page covers the publish decision; for development itself, see the development and commit rules.

When the developer says "publish" or "release", the first thing to decide is **where** to publish. Do not assume a route.

- **To the community** — release the firmware to the [AI Passport Community market](https://ai-passport.folotoy.cn). Use the publisher workflow below. See also the project completion flow (`docs/development/project-completion.md`) once it is published.
- **To Git** — publish the finished code to a version-controlled repository. The flow is: commit the finished code, then open a release for it. The release artifact may be produced by the CI/CD pipeline (**recommended**, tag-triggered via `.github/workflows/build-firmware.yml`), or it may be a local `.bin` the developer already built. Follow the repository commit and PR rules (`docs/contribution/commit-and-pr.md`) and the fork workflow (`docs/fork-guide.md`); see [`CI-build-and-release.md`](CI-build-and-release.md) for the automated build and release artifacts.
- **Neither yet** — the code is not finished and usable; keep developing, do not publish.

The workflow is driven by the official publisher skill. Running the prompt once makes the assistant install the skill from the official bundle; nothing is committed into the repository.

## Reproduce the bundled prompt

To run the publishing workflow, copy the prompt to your AI assistant. The paste-ready prompt is reproduced verbatim in the Simplified Chinese peer [`publish-to-community.zh_CN.md`](publish-to-community.zh_CN.md); it requests installing the assistant at `https://ai-passport.folotoy.cn/skills/folotoy-ai-passport-publisher.zip`, then inspecting the project, preparing bilingual title and description, choosing a cover, resolving the HTTPS Git source, authorizing through the official site, and confirming every field before uploading.

The skill's `SKILL.md` defines the exact workflow: inspect the project, prepare bilingual title and description, resolve the HTTPS Git source, prepare and validate a cover, authorize through the official site, then preview every field and obtain approval before uploading.

## What the assistant will need from you

- **Firmware**: the single merged ESP image
  `build/FoloToy-AI-Passport-full.bin`. It must pass
  `./tools/validate.sh --firmware`, including the
  [mini-program BLE compatibility contract](ble-recovery-compatibility.md).
  Never substitute the app-only `.bin` produced by `idf.py build`.
- **Cover**: a representative JPEG / PNG / WebP image (<= 10 MiB).
- **Source**: the public HTTPS Git page for the firmware repository — GitHub, Gitee, GitLab, Codeberg, or another publicly reachable HTTPS Git repository page. A fork owner publishes from their fork's source page, resolved from `git remote -v`.

## Safety and boundaries

- Upload only to `https://ai-passport.folotoy.cn`. Publishing and updating are external mutations.
- Validation, drafting, and preview that is not confirmed by the author does **not** authorize upload.
- Authorization credentials are never requested, received, or stored by the assistant. The creator registers or signs in on the official site and approves the displayed code; the assistant never handles their password.
- Never retry a rejected upload automatically. Report the server response and resolve the cause with the creator first.
- Do not weaken, bypass, or remove the BLE compatibility gate merely to make a
  community submission pass. Fix the image layout or build packaging instead.

## How the assistant installs the skill

The assistant fetches the official bundle from the URL in the prompt and follows the workflow described in its `SKILL.md`. No skill needs to be kept or committed in this repository; the prompt reproduces the official install source each time.

## After publishing: archive to plays

Once the firmware is published, ask the developer whether to archive this application
into the upstream repository's [`plays/`](../../plays/README.md) application
archive. If they agree, generate an AI-generated functional summary for the application under
`plays/<username>/<app-name>/` (bilingual `README.md` / `.zh_CN.md`). The archive is
**text-only**: record the cover image only by its file name and format as publish
metadata, and do **not** commit the cover image into `plays/` (see the convention in
[`plays/README.md`](../../plays/README.md)). Do not store the firmware `.bin` here
either. Use the `plays-archive` skill, which opens the archive PR against the
upstream `FoloToy/ai-passport`.

This keeps the release's application discoverable in-repository for later querying.
