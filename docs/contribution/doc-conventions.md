<p align="right">
  <a href="doc-conventions.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Documentation Conventions

These rules apply equally to human contributors and AI agents. Documentation is reviewed and fact-checked like code; authority follows a document's responsibility, not its author.

## Language and file layout

- English is mandatory at every maintained default Markdown path: `name.md`.
- Simplified Chinese is provided at the paired path `name.zh_CN.md`.
- Both files begin with reciprocal language links. Keep their headings, facts, examples, safety warnings, and links aligned in the same change.
- English default prose must not contain Chinese text. The `简体中文` switch label is the only allowed CJK text on an English page.
- Code, commands, paths, URLs, identifiers, and data fields remain unchanged between translations where appropriate.
- The repository check rejects an unpaired document, a missing language switch, or CJK prose in an English default file.

## Task-based context

- Every task starts with root `AGENTS.md` only.
- Follow its routing table and read only documents and source relevant to the change.
- Use `docs/README.md` for the overview and `docs/INDEX.md` for discovery.
- Update the authoritative source of a changed fact and documents that directly reference it; do not create a second source of truth.

## Responsibilities

- Upstream baseline documents cover AI Passport hardware, BSP behavior, baseline demos, engineering constraints, and acceptance methods.
- Shared contribution and engineering documents cover code style, testing, commits, CI, and AI workflows grounded in this repository's real tools.
- Fork-only product requirements, business logic, or assets stay in the fork's root README or `docs/assets/`.

## Placement

- Keep the tracked repository root limited to tool-discovery files (`AGENTS.md`, `CLAUDE.md` and their translations), an optional fork README pair, license/build manifests, and ESP-IDF configuration.
- Put project documentation and history in `docs/`, grouped by contribution, development, hardware, and software responsibility.
- Put GitHub-recognized community files, templates, issue forms, and workflows in `.github/`.
- Put reusable binary/source assets in `assets/`, project skills in `skills/`, and automation in `tools/`.
- Repository checks reject additional root Markdown. Do not add a root document merely for visibility; link it from `docs/INDEX.md` instead.

Do not create empty document scaffolding without a concrete purpose. Register added documents in `docs/INDEX.md` or their directory index and update links when moving or deleting files.

## Writing, safety, and file operations

- Explain rationale, boundaries, failure modes, and validation instead of restating source code.
- State product facts and public hardware interfaces directly; omit provenance and source-availability commentary.
- Enforce automatable rules in `tools/` and CI as well as documentation.
- Record user-visible behavior, compatibility, and release-flow changes in `docs/CHANGELOG.md`.
- Never commit credentials, tokens, keys, authorization files, private keys, personal data, internal endpoints, or unsanitized device QR parameters. Run `./tools/validate.sh --static` before committing.
- Preserve existing user changes and untracked files. Use recoverable deletion for user files, and confirm intent before deleting branches, tags, or remote references.
