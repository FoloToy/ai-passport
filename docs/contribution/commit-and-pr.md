<p align="right">
  <a href="commit-and-pr.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Commit and Pull Request Conventions

## Commits

- Use an English imperative subject and Conventional Commit form: `type(scope): description`.
- Use `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `chore`, `build`, or `ci`. Keep the subject at most 72 characters and omit the final period.
- Keep each commit independently reviewable. Describe the final diff, not the debugging journey.
- Review the complete diff, run applicable checks, and exclude credentials, generated firmware, and unrelated files.
- Update `docs/CHANGELOG.md` only for user-visible behavior, compatibility, or release-flow changes.
- Creating a commit does not authorize pushing, opening a PR, releasing, or merging.
- Write durable product constraints, data formats, architecture decisions, and acceptance criteria back to their authoritative document. Do not preserve transient debugging notes.

## Pull requests

- Use English for the PR title, following the Conventional Commit format and English imperative style.
- Write the PR body in English and complete `.github/PULL_REQUEST_TEMPLATE.md`.
- Report Build, Host tests, and Device tests separately. Put unperformed hardware work under `Unverified`.
- Pin, rotation, codec-clock, ADC, DMA, Flash-layout, and power changes require the board revision and observed hardware results before the PR is ready to merge.
- Attach a photo or screenshot for display changes and explicitly describe wiring, pin-map, persistent-format, and compatibility impacts.
