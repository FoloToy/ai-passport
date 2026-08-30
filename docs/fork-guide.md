<p align="right">
  <a href="fork-guide.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Fork Workflow

The upstream repository keeps `main` as the current FoloToy baseline. Fork-specific firmware belongs on `feature/*` branches so a fork can continuously synchronize its `main` without mixing product work into the baseline.

## Repository roles

```text
docs/                  product, contribution, development, and design documents
components/bsp/        stable board APIs and hardware implementation
main/                  LVGL menu and independent demo pages
assets/                reusable fonts, images, music, and sound effects
skills/                reusable AI-agent skills
tests/                 host-runnable logic tests
sdkconfig.defaults     reproducible ESP32-C3 defaults
```

The root `README.md` path is intentionally available to a fork owner. Upstream's project overview is `docs/README.md`, which GitHub displays when no root README exists. A fork may add its own root README to explain its product without replacing upstream documentation.

## Fork rules

- Keep fork `main` synchronized with `FoloToy/ai-passport:main`.
- On fork `main`, limit fork-owned content to a root `README.md` pair and `docs/assets/`; develop firmware and other changes on `feature/*` branches and merge by pull request.
- **Before starting a new project, create the `feature/*` branch from a base that stays aligned with `FoloToy/ai-passport:main`.** Use the latest upstream `main` as the branch-off point (e.g. fetch upstream then branch from `upstream/main`), not the fork's own `main` — the fork `main` may be stale or unable to sync. This keeps every new project on the current upstream baseline and avoids basing work on a stale or diverged fork `main`.
- Enable GitHub Actions manually after forking. The upstream-sync workflow is disabled by GitHub until the fork owner enables it.
- If development must happen directly on `main`, disable `.github/workflows/sync-main.yml` first to prevent automatic merge conflicts.

Use `docs/assets/` for architecture notes, product design, and images that supplement a fork's README. Upstream keeps that directory empty except for `.gitkeep`; fork-private content must not be proposed back to upstream.

Documentation and experience follow the same split. Fork-specific product customization (architecture notes, product design, fork-only assets) stays in the fork under `docs/assets/` and is not proposed back upstream. General, upstream-benefiting documentation or experience improvements — durable facts, reusable interfaces, build or release-flow improvements that help any AI Passport user — are submitted back upstream as a pull request. The `plays/` application archive and the post-release experience notes belong upstream and are proposed back as pull requests. Use the `experience-pr` and `plays-archive` skills for post-release work; see `docs/development/project-completion.md`.

All fork documentation follows the repository language rule: English at the default `.md` path and Simplified Chinese at `.zh_CN.md`, with reciprocal switches.
