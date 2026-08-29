<p align="right">
  <a href="readme-update.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Action E: Update the Root README

This action updates the fork's root `README.md` on the relevant branches to
reflect the newly released or archived application. It is one of the six optional
closing actions listed in the [project completion](../project-completion.md).

The root README path is intentionally reserved for the fork owner. Upstream's
project overview lives at `docs/README.md`; a fork may add its own root README to
explain its product without replacing upstream documentation.

The fork keeps `main` synced with upstream and puts product work on `feature/*`
branches, so root READMEs exist on multiple branches. Handle each branch's root
README independently — the `main` README and a `feature/*` branch README are
separate decisions.

## When this is recommended

The README update is an **optional** action like the other five, and it is also
the default companion to archiving: when the application is archived to `plays/`
(action D), the README sync runs as part of that action. Archiving itself is
optional — the developer may decline — but whenever a project is completed, the
README should be refreshed on the hosting branch and on fork `main` so the
application is registered where it is developed.

## Rules

- Only touch fork-owned root READMEs (`README.md` / `README.zh_CN.md`); do not
  modify the upstream project overview at `docs/README.md`.
- Check the root README on each relevant branch (`main` and the current
  `feature/*` branch), not just one branch.
- The fork `main` root README is the **catalog of the fork's projects**: it
  **fully includes** the content of each project's own README — a complete
  description of what the application does and how to use it (its interactions,
  modes, keys, persistence, and notes) — not a one-line intro followed by a
  branch link. Pull the content from the hosting branch's README.
- The fork root README and the hosting branch's root README are fork-owned
  content. Commit them directly (merge) rather than opening a PR; open a PR only
  when the change is meant to go upstream.
- Follow the repository language rule: English at the default `.md` path and
  Simplified Chinese at the paired `.zh_CN.md`, aligned in the same change.

## Steps

1. Confirm consent and a GitHub channel (GitHub MCP, a GitHub skill, or `gh`).
2. On the hosting `feature/*` branch: create the bilingual README pair if it is
   missing, or update it to add or refresh the application's own description.
3. On fork `main`: update the root README pair so the released application is
   discoverable from the repository landing page, fully including the hosting
   branch's README content.
4. Commit the README updates directly to the branch / fork `main` (fork-owned
   content); do not open a PR for this unless it is an upstream change.

## Related documents

- Fork workflow and root README ownership: [fork-guide.md](../../fork-guide.md)
- Application archive skill: [plays-archive](../../../skills/plays-archive/SKILL.md)
- Documentation conventions: [doc-conventions.md](../../contribution/doc-conventions.md)
