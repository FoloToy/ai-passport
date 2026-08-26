<p align="right">
  <a href="README.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Plays

This directory is the in-repository archive of the applications (plays) built
for the AI Passport. It is for **querying** what each application does and how it
works, using an AI-generated functional summary per application. It is linked to
the community publishing flow: after publishing a firmware
([`docs/development/publish-to-community.md`](../docs/development/publish-to-community.md)),
the assistant asks whether to archive the application here.

## Directory convention

Each application gets its own subdirectory, named after the application in
lowercase-kebab-case. Add an application archive only when it is published or
ready to be recorded; do not pre-create empty scaffolding.

```
plays/<app-name>/
  README.md / README.zh_CN.md   # AI-generated bilingual functional summary
  <app-name>-cover.<webp|png|jpg>  # cover image, committed (<= 10 MiB)
```

## What the per-application README contains

The per-application `README.md` (and its Simplified Chinese peer) is an
AI-generated functional summary written for later querying, not a publishing
artifact. It records:

- Application name and one-line positioning.
- What the app does and its feature list.
- Interaction and gameplay (buttons, screens, flow).
- The source branch or directory it lives in (for example a `demo/*` branch or
  `main/`).
- The cover image file name and format.

Write it by summarizing the application's implementation and behavior, in
English at the default `.md` path and Simplified Chinese at the paired
`.zh_CN.md`, aligned in the same change.

## Cover image

Place the cover at `plays/<app-name>/<app-name>-cover.<webp|png|jpg>`, committed
to the repository (like `docs/assets/brand`). Keep it representative and under
10 MiB.

## Firmware

Do **not** store the merged firmware binary here. The `.bin` is a build/publish
artifact produced by the build flow, not an in-repository asset.

## Related

- Repository overview and demo branches: [`../docs/README.md`](../docs/README.md)
- Software design index: [`../docs/software-design/README.md`](../docs/software-design/README.md)
