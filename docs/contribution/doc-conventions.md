<p align="right">
  <a href="doc-conventions.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Documentation Conventions

These rules apply equally to human contributors and AI agents. Documentation is reviewed and fact-checked like code; authority follows a document's responsibility, not its author.

## Language and file layout

- The language exemption applies only to root README files. `README.md` may use any language, and any number of `README.<tag>.md` versions may coexist; when multiple versions exist, every language link must resolve to an existing file and be reciprocal.
- Every other maintained Markdown document, including other root Markdown such as `AGENTS.md`, uses English at its default `name.md` path.
- Simplified Chinese is provided at the paired path `name.zh_CN.md`.
- For every such pair, both files begin with reciprocal language links. Keep their headings, facts, examples, safety warnings, and links aligned in the same change.
- English default prose must not contain Chinese text. The `简体中文` switch label is the only allowed CJK text on such an English page.
- Code, commands, paths, URLs, identifiers, and data fields remain unchanged between translations where appropriate.
- The repository check rejects an unpaired document, a missing language switch, or CJK prose in an English default file, except for root README files.

### GitHub community-document links

GitHub also renders `.github/CONTRIBUTING`, `CODE_OF_CONDUCT`, `SECURITY`, and
`SUPPORT` documents outside their file view. In both language versions, put a
Markdown language switch on the first nonempty line, outside HTML blocks, with
a repository-root path, for example
`[简体中文](/.github/CODE_OF_CONDUCT.zh_CN.md)`, and use repository-root paths for
other internal links, such as `/docs/README.md`. Bare filenames in HTML language
switches can lose the `.github/` directory in the repository overview. Do not
hardcode the upstream owner or `main` into these document links: root-relative
links keep the current repository and branch. See GitHub's
[relative-link rules](https://docs.github.com/en/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax#relative-links).
The repository gate checks these community-document links; after deployment,
also click both language directions from the overview and individual file views.

## Vendored third-party documentation

Preserve upstream documentation when copying a third-party component into the
repository. To exempt its original Markdown from local-link and bilingual
checks, explicitly register the component's directory in `VENDORED_DOC_ROOTS`
in [`tools/check_repo.py`](../../tools/check_repo.py). The registry is empty by
default. For example, after adding a component at `components/vendor_audio`:

```python
VENDORED_DOC_ROOTS: tuple[str, ...] = (
    "components/vendor_audio",
)
```

Register an existing, concrete component directory using a repository-relative
path with `/` separators. Empty paths, the repository root, absolute paths,
`.` or `..` segments, and symlink directories are rejected. Matching uses whole
path segments: this entry does not exempt `components/vendor_audio_extra`.
Files linked outside the registered directory are not exempt. Do not register
broad first-party trees such as `components` or `docs`.

Only upstream Markdown under the registered roots skips `check_markdown_links`
and `check_document_languages`. Keep those files in the normal repository scan:
secret patterns, unsanitized device QR links, and merge-conflict markers still
fail validation. Do not implement these exceptions through `.gitignore`,
`git_files()`, or `text_files()` filters.

Record the component's upstream source URL, pinned version or commit, license,
and any local modifications in a project-maintained English/Chinese document
pair outside its exempt directory. Project-authored integration guides remain
subject to the normal language and link rules. Adding an exemption does not
require rewriting or translating the original upstream documents.

## Task-based context

- Every task starts with root `AGENTS.md` only.
- Follow its routing table and read only documents and source relevant to the change.
- Use `docs/README.md` for the overview and `docs/README.md` for discovery.
- Update the authoritative source of a changed fact and documents that directly reference it; do not create a second source of truth.

## Responsibilities

- Upstream baseline documents cover AI Passport hardware, BSP behavior, baseline demos, engineering constraints, and acceptance methods.
- Shared contribution and engineering documents cover code style, testing, commits, CI, and AI workflows grounded in this repository's real tools.
- Fork-only product requirements, business logic, or assets stay in the fork's root README or `docs/assets/`.

## Placement

- Keep the tracked repository root limited to the AI entry point (`AGENTS.md` and its maintained Simplified Chinese companion `AGENTS.zh_CN.md`), optional fork README files, license/build manifests, and ESP-IDF configuration.
- Put project documentation and history in `docs/`, grouped by contribution, development, hardware, and software responsibility.
- Put GitHub-recognized community files, templates, issue forms, and workflows in `.github/`.
- Put reusable binary/source assets in `assets/`, project skills in `skills/`, and automation in `tools/`.
- Repository checks reject additional root Markdown. Do not add a root document merely for visibility; link it from `docs/README.md` instead.
- Root README scope follows the branch model. The `main` README catalogs the applications hosted on other branches; when development happens directly on `main`, it describes `main`'s own functionality instead. Every other branch's root README describes only that branch's application and must not list or advertise other branches' applications.

Do not create empty document scaffolding without a concrete purpose. Register added documents in `docs/README.md` or their directory index and update links when moving or deleting files.

## Changelog ownership

- Ordinary feature, application, and documentation pull requests leave `docs/CHANGELOG.md` and `docs/CHANGELOG.zh_CN.md` unchanged. They describe user-visible behavior, compatibility, and release-flow impact in the pull-request body and update the authoritative product or application documentation.
- During release preparation, the release maintainer reviews merged pull requests since the previous release, keeps only user-visible changes, and updates both changelog languages together before creating the tag.
- The release-preparation change places the released entries under a version-and-date heading and leaves a fresh `Unreleased` section. Existing entries under `Unreleased` are pending input for the next release and must be checked against the merged changes rather than copied blindly.

## Writing, safety, and file operations

- Explain rationale, boundaries, failure modes, and validation instead of restating source code.
- State product facts and public hardware interfaces directly; omit provenance and source-availability commentary.
- Enforce automatable rules in `tools/` and CI as well as documentation.
- Never commit credentials, tokens, keys, authorization files, private keys, personal data, internal endpoints, or unsanitized device QR parameters. Run `./tools/validate.sh --static` before committing.
- Preserve existing user changes and untracked files. Use recoverable deletion for user files, and confirm intent before deleting branches, tags, or remote references.
