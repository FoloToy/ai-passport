<p align="right">
  <a href="publish-to-community.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Action A: Publish to the Community Market

This action releases the firmware to the AI Passport community market. It is one
of the six optional closing actions listed in the
[project completion](../project-completion.md).

The workflow is driven by the official publisher skill. Running the prompt once
makes the assistant install the skill from the official bundle; nothing is
committed into the repository.

## Inputs

- A single merged ESP `.bin` flashed from `0x0`, built and verified with
  `./tools/validate.sh --firmware` (this produces and verifies the merged full
  image; do not substitute `idf.py build`, which is only for day-to-day
  incremental compilation).
- A representative cover image (JPEG / PNG / WebP, up to 10 MiB).
- The public HTTPS Git page for the firmware repository, resolved from
  `git remote -v`.

## Output

These values form the [published profile](../project-completion.md#published-profile) that
the other closing actions reuse:

- Application name.
- Bilingual publish title and description.
- Cover image.
- Source address.

## Steps

1. Install the publisher skill from the official bundle.
2. Inspect the project and prepare the bilingual title and description.
3. Resolve the HTTPS Git source.
4. Prepare and validate the cover.
5. Authorize through the official site.
6. Preview every field and obtain explicit approval before uploading.
7. Upload and report the response.

## Safety and boundaries

- Upload only to `https://ai-passport.folotoy.cn`. Publishing and updating are
  external mutations.
- Validation, drafting, and preview that is not confirmed by the developer does
  not authorize upload.
- The assistant never requests, receives, or stores authorization credentials.
- Never retry a rejected upload automatically; report the response and resolve
  the cause with the developer first.

## Related documents

- Community publishing reference: [publish-to-community.md](../publish-to-community.md)
