<p align="right">
  <a href="commit-and-pr.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Commit and Pull Request Conventions

## Commits

- Use an English imperative subject and Conventional Commit form: `type(scope): description`.
- Use `feat`, `fix`, `docs`, `refactor`, `perf`, `test`, `chore`, `build`, or `ci`. Keep the subject at most 72 characters and omit the final period.
- Commit in fine-grained units: one commit carries one logical change that can be
  reviewed and reverted on its own. Split by unit, so a driver fix, its host test,
  and the documentation update are three commits rather than one.
- Work through a long task unit by unit. Commit each unit when it is finished
  instead of batching the whole task into a single commit at the end.
- Write a detailed English body, not a subject-only message: state what the final
  diff changes, why the change is needed, and what was actually verified. Describe
  the final diff, not the debugging journey.
- Run the applicable checks before each commit; a full ESP-IDF build is not
  required between units. Run the complete gate once at the end of the task.
- Review the complete diff and exclude credentials, generated firmware, and
  unrelated files.
- Do not edit the paired changelog files in an ordinary feature, application, or documentation commit. Describe user-visible behavior, compatibility, and release-flow impact in the pull-request body; only a release-preparation change maintained before tagging aggregates those merged changes into both files.
- Creating a commit does not authorize pushing, opening a PR, releasing, or merging.
- Write durable product constraints, data formats, architecture decisions, and acceptance criteria back to their authoritative document. Do not preserve transient debugging notes.

## Pull requests

- Use English for the PR title, following the Conventional Commit format and English imperative style.
- Write the PR body in English and complete `.github/PULL_REQUEST_TEMPLATE.md`.
- Report Build, Host tests, and Device tests separately. Put unperformed hardware work under `Unverified`.
- Pin, rotation, codec-clock, ADC, DMA, Flash-layout, and power changes require the board revision and observed hardware results before the PR is ready to merge.
- Attach a photo or screenshot for display changes and explicitly describe wiring, pin-map, persistent-format, and compatibility impacts.
- In a PR opened to the upstream `FoloToy/ai-passport` project, do not add or modify the repository-root `README.md` / `README.zh_CN.md` — the root README is fork-owner reserved content (upstream keeps its overview at `docs/README.md`).
