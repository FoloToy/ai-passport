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

## Rules

- Only touch fork-owned root READMEs (`README.md` / `README.zh_CN.md`); do not
  modify the upstream project overview at `docs/README.md`.
- Check the root README on each relevant branch (`main` and the current
  `feature/*` branch), not just one branch.
- Follow the repository language rule: English at the default `.md` path and
  Simplified Chinese at the paired `.zh_CN.md`, aligned in the same change.

## Steps

1. Confirm consent and a GitHub channel (GitHub MCP, a GitHub skill, or `gh`).
2. For each relevant branch, check whether a fork-owned root README exists.
3. If it exists, update it to include the newly archived application.
4. If it does not exist, create a fork-owned root README describing the product.

## Related documents

- Fork workflow and root README ownership: [fork-guide.md](../../fork-guide.md)
- Application archive skill: [plays-archive](../../../skills/plays-archive/SKILL.md)
- Documentation conventions: [doc-conventions.md](../../contribution/doc-conventions.md)
