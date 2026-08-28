<p align="right">
  <a href="doc-conventions.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Documentation Conventions

These rules apply to human contributors and AI agents. A document owns only the facts assigned to it by `docs/INDEX.md`.

## Language and pairing

- Every maintained Markdown default path, `name.md`, contains English prose and has a Simplified Chinese `name.zh_CN.md` pair.
- Both files begin with reciprocal language links and keep heading levels, facts, commands, code blocks, safety warnings, and local-link targets aligned in the same change.
- Commands, paths, URLs, identifiers, and data fields remain byte-identical when translation does not change their meaning.
- The English file contains no CJK prose except the `简体中文` language-switch label.
- `python3 tools/check_repo.py` rejects missing pairs, missing language links, CJK English prose, broken links, and structural drift on mandatory AI-path documents.

## Context and authority

- Every task begins with root `AGENTS.md`; the task routing table names every additional mandatory document.
- `docs/README.md` owns the capability contract. `docs/hardware-design/specifications.md` owns product hardware. The hardware guide owns engineering constraints and device acceptance. `bsp_pins.h` owns firmware constants.
- Update the authoritative document and every direct reference in the same change. Do not create a second narrative source for the same rule or hardware fact.
- The current checkout is self-contained. Maintained instructions must not require a remote branch, community archive, website content, private file, or untracked local configuration.

## Placement

- Root Markdown is limited to `AGENTS.md`, `AGENTS.zh_CN.md`, `CLAUDE.md`, and `CLAUDE.zh_CN.md`.
- Product and engineering documents live in `docs/`; GitHub-recognized policy, templates, forms, and workflows live in `.github/`; automation lives in `tools/`.
- Application assets live in `main/assets/`, created only in a change that adds at least one tracked asset. Reusable board resources consumed by BSP code live in `components/bsp`.
- Do not create an empty directory, placeholder README, archive, experience log, brand library, publishing guide, fork guide, or project-operation skill in this firmware baseline.
- Register every added document in `docs/INDEX.md` or its directory index and remove every inbound link when deleting a document.

## Writing and verification

- State exact triggers, actions, prohibited actions, outputs, failure states, and validation commands. Replace “as needed,” “appropriate,” “relevant,” and similar qualifiers with a measurable condition.
- State an unverified hardware claim as `Unverified`, identify the missing measurement, and prohibit code from depending on it. Ask the user before turning it into a product fact.
- Explain ownership, rationale, failure mode, and acceptance; do not restate source code line by line.
- Only the release maintainer updates `docs/CHANGELOG.md`, and only for released upstream baseline behavior or compatibility.
- Run `./tools/validate.sh --static` after every documentation set is complete.

## Security and file operations

- Never store credentials, tokens, authorization files, private keys, personal data, internal endpoints, device QR secrets, or unsanitized logs in files, tests, examples, commits, or PR text.
- A sanitized device URL uses literal placeholders: `https://example.invalid/?s=<secret>&k=<key>`.
- Preserve existing user changes and untracked files. Delete only targets explicitly authorized by the user or required by the accepted task scope.
- Tracked deletions remain recoverable from Git history. Branch, tag, remote-reference, and history-rewrite operations require explicit authorization.
