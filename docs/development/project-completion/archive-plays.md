<p align="right">
  <a href="archive-plays.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Action D: Archive the Application to plays

This action archives a published application into the upstream `plays/`
application archive so it is discoverable in-repository for later querying. It
is one of the six optional closing actions listed in the
[project completion](../project-completion.md).

The workflow is driven by the `plays-archive` skill.

## Inputs

- Application name (lowercase-kebab-case).
- [Published profile](../project-completion.md#published-profile): bilingual title and
  description, and the source address.

## Steps

1. Confirm consent and a GitHub channel (GitHub MCP, a GitHub skill, or `gh`).
2. Generate a bilingual AI-functional summary under
   `plays/<username>/<app-name>/` (`README.md` / `.zh_CN.md`), merging the root
   README when one exists.
3. Record the publish metadata — the bilingual title and description and the
   source address — which include the cover image by file name and format, but
   do not commit the cover image itself. The archive is text-only.
4. Handle each branch's root README independently (see
   [readme-update.md](./readme-update.md) for the required README sync).
5. Commit only the summary on a dedicated branch; do not store the firmware
   `.bin` or the cover image.
6. After review, open the archive PR against the upstream project.

## Safety

- Never store the merged firmware `.bin` or the cover image in the archive; the
  archive is text-only, and both are build/publish artifacts.
- Do not submit before developer review and consent.

## Related documents

- Application archive convention: [plays/README.md](../../../plays/README.md)
- Skill: [plays-archive](../../../skills/plays-archive/SKILL.md)
- README update (required): [readme-update.md](./readme-update.md)
