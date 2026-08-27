<p align="right">
  <a href="after-release.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Post-Release Follow-up

Once a release is complete — whether it was published to the AI Passport
community or to Git — finish the loop with the follow-up actions on this page.
Publishing itself is described in
[publish-to-community.md](publish-to-community.md); this page is the overview of
what happens after that.

The follow-up works on four independent tracks. Track 1 is a device check; the
rest are driven by repository skills and follow the same safety and consent
gates. None re-publishes firmware.

## Track 1: Re-flash the released full build and verify

After the release is complete, download the release's **merged full firmware**
(`FoloToy-AI-Passport-full.bin`, the flashable complete build from `0x0`), flash
it to a device, and confirm it runs normally. Do not treat a successful build or
upload as hardware validation: this step proves the artifact the release actually
points to boots and works on real hardware. The artifact comes from the release
assets (the CI/CD `full.bin`) or, for a Git release with no CI artifact, the
local `full.bin` the developer built. If it does not run, stop and fix before the
next tracks. See [`CI-build-and-release.md`](CI-build-and-release.md) for the
artifact and flashing.

## Track 2: Archive the application to plays

After publishing, ask the developer whether to archive this application into the
upstream `FoloToy/ai-passport` repository's `plays/` application archive. If
agreed, generate an AI-functional summary under
`plays/<username>/<app-name>/` (bilingual `README.md` / `.zh_CN.md`) and add the
cover image as
`plays/<username>/<app-name>/<app-name>-cover.<webp|png|jpg>`. Commit only the
summary and cover; do not store the firmware `.bin` here. See
[`../../plays/README.md`](../../plays/README.md).

Before generating the summary, check the root README of the `main` and current
branches (`plays-archive` skill): if a README exists, merge it into the summary;
if not, summarize directly. After archiving, handle each branch independently:
create the root README on a branch that lacks one, and prompt the developer to
update the README on a branch that already has one.

## Track 3: Collect suggestions and file issues

Run the `issue-suggestions` skill to gather the releasing developer's own
improvement points and file the worthwhile ones as feature request issues
against the upstream `FoloToy/ai-passport` project.

1. Confirm the developer agrees to start the follow-up (project-private content).
2. Confirm a GitHub channel is available (GitHub MCP, a GitHub skill, or `gh`);
   otherwise hand the draft to the developer to paste.
3. Collect, deduplicate, categorize, and match against existing issues.
4. Draft a feature request issue and wait for explicit approval before applying.

## Track 4: Collect development experience and submit a PR

Run the `experience-pr` skill to capture durable, reusable learnings about the
fork's own `docs/` differences from upstream and propose them as a documentation
PR to the upstream `FoloToy/ai-passport` project.

1. Confirm the developer agrees to start the follow-up (project-private content).
2. Confirm a GitHub channel is available (GitHub MCP, a GitHub skill, or `gh`);
   otherwise hand the draft to the developer to paste.
3. Collect reusable experience from the fork's `docs/` differences, route it
   (general, upstream-benefiting experience goes upstream; fork-specific
   customization stays in the fork per `fork-guide.md`), and store it as a new
   entry under `docs/experiences/<username>/` (named after the entry's content
   summary in lowercase-kebab-case, plus the `.zh_CN.md` peer, grouped by the
   contributing developer's GitHub username), linking it from the
   [experience index](experience-notes.md), on a dedicated branch based on the
   latest upstream `main`, so the current checkout
   stays untouched.
4. Present the change for review, then commit, push to the fork, and open a PR
   against the upstream `FoloToy/ai-passport` only after explicit approval.

## Shared safety and consent gates

Track 1 is a device check and needs no skill or GitHub channel. Tracks 2–4
follow the same non-negotiable rules:

- Confirm consent before starting; this work touches project-private content.
- Confirm a GitHub channel (GitHub MCP, a GitHub skill, or `gh`) before any
  submission; if none is available, generate content for manual pasting and stop.
- Do not submit (issue or PR) until the developer has reviewed and authorized it.
- Do not commit on or modify the developer's current branch; carry the PR change on a
  dedicated branch or worktree.
- Never include credentials, device QR secrets, private device links, personal
  data, or unsanitized logs.

## Related documents

- Firmware publishing: [publish-to-community.md](publish-to-community.md)
- Application archive: [`../../plays/README.md`](../../plays/README.md)
- Filing issues: [file-issues.md](file-issues.md)
- Development experience: [experience-notes.md](experience-notes.md)
- Issue and contribution rules: [../contribution/commit-and-pr.md](../contribution/commit-and-pr.md)
