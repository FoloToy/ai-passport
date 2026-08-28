<p align="right">
  <a href="post-release-follow-up.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Post-Release Follow-up for the AI Passport Publishing Flow

Captured after publishing the **Voice Keychain** firmware to the AI Passport
community market. This entry records the reusable lessons from closing out a
fork's release with the post-release follow-up tracks, which apply to any AI
Passport release.

## Confirm the publish destination before routing

The publishing document says to publish "to the community" or "to Git", but a
developer often says just "publish the project". Ask the destination first, and
route to the matching workflow, before assuming either. This two-way split
(community market vs a version-controlled repository) prevents the wrong
workflow from running.

## The community upload needs three inputs, and the split matters

The publisher workflow requires: a single merged firmware `.bin` (flashed from
`0x0`), a representative cover image (<= 10 MiB), and a public HTTPS Git source
page. Two of these can be prepared from the existing repository, and the source
URL should be resolved from `git remote -v` (converting SSH to HTTPS when
ambiguous). The merged image from `idf.py merge-bin` only bundles bootloader,
partition-table, and app — it does **not** include the SPIFFS audio data
partition. That partition's image is produced separately and is easy to omit
from a release, leaving a firmware that boots a UI but has no sound. A release
should either merge that data partition into `full.bin` or publish it alongside
with clear flash addresses.

## Authorize through the official site; never handle credentials

The publisher skill uses a device-code flow. Running `authorize` opens the
official creator page and produces a ten-minute code; the creator registers or
signs in on the site and approves the displayed code. The assistant never
requests, receives, or stores the creator's password, and never retries a
rejected upload automatically.

## Post-release follow-up has its own consent gates

After publishing, three independent tracks can follow: archive the application
to `plays/`, gather improvement suggestions as an upstream issue, and submit
reusable development experience as an upstream documentation PR. Each touches
project-private content and each requires explicit developer consent, an
available GitHub channel, and no submission before review. All three keep the
change off the developer's current branch.

## Generalization for the next release

- Always confirm the destination (community vs Git) before publishing.
- Verify the merged image includes (or is published alongside) the data
  partition needed for sound; do not assume "one file = whole device".
- Route each post-release track to upstream only when the learning is general;
  keep fork-specific customization local per `fork-guide.md`.

## Related documents

- `docs/development/publish-to-community.md` — the publishing workflow.
- `docs/development/project-completion.md` — the project completion flow and its gates.
- `docs/development/experience-notes.md` — index for this entry.
