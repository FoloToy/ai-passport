<p align="right">
  <a href="release-update.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Action B: Publish to Git and Update the Release

This action publishes the firmware or code to a version-controlled repository
and, when intended, updates the GitHub/GitLab release. It is one of the six
optional closing actions listed in the [project completion](../project-completion.md).

This is the Git publishing path, not the community path. Confirm the destination
first; see [publish-to-community.md](./publish-to-community.md) for the community
market.

## Steps

Each step is an external, authorizing mutation. Confirm each one with the
developer separately — do not treat a single up-front confirmation as covering
commit, push, tag, and release.

1. Commit the change and push it to the fork (`origin`) — confirm separately.
2. Create and push a tag to trigger the release workflow — confirm separately.
3. Let the tagged build produce the merged firmware `.bin`.
4. Create or update the GitHub/GitLab release with the artifact — confirm
   separately. The workflow sets the default release title to the version/tag
   name; after the release is up, refine it to the project feature name plus the
   version number.
5. Write release notes in English (and a Simplified Chinese version where the
   project is bilingual) covering what is new, how to build, and how to use.
6. Verify the released full build on hardware (see
   [Post-release hardware verification](../project-completion.md#post-release-hardware-verification)).

## Rules

- Follow the repository commit and pull-request rules
  ([commit-and-pr.md](../../contribution/commit-and-pr.md)).
- Follow the fork workflow ([fork-guide.md](../../fork-guide.md)).
- A tag-triggered build runs `build-firmware.yml`, which publishes the release
  only for a tag. See [CI-build-and-release.md](../CI-build-and-release.md).
- For day-to-day compilation prefer `idf.py build` (fast, incremental); use
  `./tools/validate.sh --firmware` only when the merged, byte-verified `0x0`
  full image is needed, such as before a release or delivery.
- The workflow creates the release with a default title of the version/tag name
  (from `softprops/action-gh-release` and `github.ref_name`). After the release
  is published, refine the title to the project feature name plus the version
  number — for example `Voice Keychain v1.2.0`. The version is the tag, and the
  feature name is the application's publish name from the shared
  [publish profile](../project-completion.md#published-profile).
- Release notes must explain the build to a user who has not read the
  repository: what is new, how to build, and how to use.

## Related documents

- Tagged firmware builds and releases: [CI-build-and-release.md](../CI-build-and-release.md)
- Commit and pull-request rules: [commit-and-pr.md](../../contribution/commit-and-pr.md)
- Fork workflow: [fork-guide.md](../../fork-guide.md)
