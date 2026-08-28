<p align="right">
  <a href="experience.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Action C: Publish Experience

This action captures reusable, durable development experience from a release and
proposes it as a documentation pull request to the upstream project. It is one
of the six optional closing actions listed in the [project completion](../project-completion.md).

The workflow is driven by the `experience-pr` skill.

## Focus

Capture the fork's own `docs/` differences from upstream — the documents the
developer created or changed on this fork. Extract only durable, reusable
learnings:

- What the fork documents or changes that upstream does not, and why.
- Hardware facts, interfaces, timings, resource budgets, or failure behavior.
- Build, validation, or release-flow improvements.
- Generalizations that apply to the next release.

## Route

Decide where each learning belongs before submitting:

- **Upstream the reusable, general experience** — learnings that benefit any
  user and belong in the upstream baseline. Submit as a PR to the upstream
  project.
- **Keep fork-specific customization in the fork** — product-customized content,
  fork-private business rules, or fork-only assets. Do not submit these
  upstream; record them locally.

## Steps

1. Confirm consent and a GitHub channel (GitHub MCP, a GitHub skill, or `gh`).
2. Compare the fork to upstream to find the `docs/` differences.
3. Extract and route the reusable experience.
4. Write a single entry under `docs/experiences/<username>/` (one `.md` file plus
   its `.zh_CN.md` peer), named after the entry's content summary in
   lowercase-kebab-case, and link it from the experience index.
5. Present the change for review, then commit, push to the fork, and open a PR
   only after explicit approval.

## Related documents

- Experience index: [experience-notes.md](../experience-notes.md)
- Skill: [experience-pr](../../../skills/experience-pr/SKILL.md)
- Fork workflow: [fork-guide.md](../../fork-guide.md)
