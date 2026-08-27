<p align="right">
  <a href="CI-validation.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Pull Request Validation

`.github/workflows/ci.yml` runs for pull requests, pushes to `main`, and manual dispatch. Local development and CI share `tools/validate.sh`.

## Jobs

- **Repository and host checks:** validates English-default bilingual Markdown, relative links, full-SHA Actions, issue forms, the dependency lock, conflict markers, and likely sensitive data; then runs `actionlint` and host tests.
- **ESP-IDF 5.5.3 firmware:** runs `./tools/validate.sh --firmware` for ESP32-C3 in a fresh isolated build/configuration directory, verifies the build and merged `0x0` image contents/offsets, and retains the artifact for seven days.

The workflow has only `contents: read` and uses no repository secrets, so it can validate fork pull requests. Every GitHub Action is pinned to a full commit SHA.

## Reproduce locally

```bash
./tools/validate.sh --static
source <path-to-esp-idf-v5.5.3>/export.sh
./tools/validate.sh --firmware
```

Follow the [environment bootstrap](environment-setup.md) if ESP-IDF 5.5.3 is
not installed. Reproduce a CI failure with the same mode locally. Do not
maintain duplicate validation commands inside the workflow.
