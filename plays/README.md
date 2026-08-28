<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Plays

This directory, in the upstream `FoloToy/ai-passport` repository, is the
application archive of the plays built for the AI Passport. It is for
**querying** what each application does and how it works, using an AI-generated
functional summary per application. Use the
[`INDEX.md`](INDEX.md) to discover the archived plays and jump to a
per-application summary. It is linked to the community publishing flow: after
publishing a firmware
([`docs/development/publish-to-community.md`](../docs/development/publish-to-community.md)),
the assistant asks whether to archive the application here, proposing it back to
upstream.

## Before developing a new play

Before starting a new application, check `plays/` for an existing or reference
project to build on instead of from scratch:

- List the archived applications under `plays/` (across contributor folders) and
  read their functional summaries to see whether one already covers the idea.
- Reuse applicable design ideas, interaction patterns, or state models from an
  existing play rather than re-inventing them.
- When none exists, note that a new `plays/<username>/<app-name>/` archive will be
  created later, when the application is published.

Each play subdirectory is an archive of a real, working application; its summary
is the starting point for deciding whether to extend or reference it. Alongside
the application archive, also check
[`docs/development/experience-notes.md`](../docs/development/experience-notes.md)
for previously recorded, reusable experience from other developer runs.

## Directory convention

Archives are grouped by the contributor who published the application, then by
the application itself, so the collection is organized by author rather than
flattened. Each application gets its own subdirectory under its contributor's
folder, both in lowercase-kebab-case. Add an application archive only when it is
published or ready to be recorded; do not pre-create empty scaffolding.

```
plays/<username>/<app-name>/
  README.md / README.zh_CN.md   # AI-generated bilingual functional summary
  <topic>-guide.md (+ .zh_CN.md)  # optional manual / how-to, not experience
```

`<username>` is the contributor's GitHub username (lowercase-kebab-case, e.g.
`shinku-chen`), and `<app-name>` is the application name (lowercase-kebab-case).
A single contributor can have several applications under their own folder; the
folder splits by author to keep related submissions together instead of spreading
them flat across `plays/`.

A play archive is **text-only**: it stores the application's **introduction and
manual** — the README functional summary and optionally a how-to guide for that
app. It does **not** store the cover image (recorded only by file name and format
as publish metadata) or reusable development experience; post-release experience
entries belong under [`docs/experiences/<username>/`](../docs/experiences/).

## What the per-application README contains

The per-application `README.md` (and its Simplified Chinese peer) is an
AI-generated functional summary written for later querying, not a publishing
artifact. It records:

- The **publish title and description** the developer submitted when publishing
  to the community (bilingual).
- Application name and one-line positioning.
- What the app does and its feature list.
- Interaction and gameplay (buttons, screens, flow).
- Source of the application, given as the **source address the developer
  submitted when publishing** (the HTTPS Git source page), so it can be located
  precisely.
- The cover image file name and format, recorded as publish metadata only — the
  cover image itself is **not** committed (the archive is text-only).

Write it by summarizing the application's implementation and behavior, in
English at the default `.md` path and Simplified Chinese at the paired
`.zh_CN.md`, aligned in the same change.

## Cover image

The cover image is a publish artifact, not repository content. Do **not** commit
the cover image into `plays/`; record only its file name and format in the
summary as publish metadata. The image itself lives with the community
publication.

When generating a cover for the community publication (not for the archive), use
the official product references under
[`docs/assets/brand/`](../docs/assets/brand/README.md). Always pass a reference
(e.g. `ai-passport-front.png` or a colorway shell render) as input to the
generation call, keep its shell, buttons, ports, and key-ring hole as they are,
and redraw only the reference's screen region into the play's actual on-screen
content. Keep the screen's size, aspect ratio, corners, and position inside the
shell identical to the reference so the play content appears inside the real
AI Passport device rather than as a bare or free-floating image.

## Firmware

Do **not** store the merged firmware binary here. The `.bin` is a build/publish
artifact produced by the build flow, not an in-repository asset.

## Related

- Archive index: [`INDEX.md`](INDEX.md)
- Repository overview and demo branches: [`../docs/README.md`](../docs/README.md)
- Software design index: [`../docs/software-design/README.md`](../docs/software-design/README.md)
