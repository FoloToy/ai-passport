<p align="right">
  <a href="build-and-test.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Build and Test

Use ESP-IDF 5.5.3. On a clean machine or when the toolchain is missing, follow the [environment bootstrap](environment-setup.md) first.

## Development build

```bash
source <path-to-esp-idf-v5.5.3>/export.sh
idf.py --version              # must report ESP-IDF v5.5.3
idf.py set-target esp32c3     # fresh checkout or changed target
idf.py build                  # incremental application build
idf.py flash monitor          # incremental development flash and logs only
idf.py fullclean              # remove generated build state only
```

`idf.py fullclean` does not fully synchronize an existing `sdkconfig` with changed defaults. Preserve intentional local settings, then run `idf.py set-target esp32c3` when the target or tracked defaults must be regenerated.

The tracked `dependencies.lock` pins Managed Component resolution. After changing an `idf_component.yml`, regenerate the lock with ESP-IDF 5.5.3, review version changes, and commit it with the manifest. An ordinary build must not leave an unexplained lock-file diff.

## Host and static checks

Run hardware-independent tests directly while iterating:

```bash
./tools/run-host-tests.sh
```

Run the complete static gate before delivery:

```bash
./tools/validate.sh --static
```

The static gate checks repository structure, bilingual Markdown pairs, links, pinned Actions, issue forms, sensitive-content patterns, conflict markers, host logic tests, and workflow syntax. Workflow lint requires `actionlint`; install the verified version with `export ACTIONLINT_BIN="$(./tools/install-actionlint.sh)"`. If `actionlint` is unavailable, repository and host-test stages still run, workflow lint reports `NOT RUN`, and the static command exits with failure.

## Firmware delivery

The delivery artifact is the byte-verified merged image `build/FoloToy-AI-Passport-full.bin`, flashed at offset `0x0`:

```bash
./tools/validate.sh --firmware
```

Firmware validation uses a fresh temporary build directory and an isolated `sdkconfig` generated from `sdkconfig.defaults`. It builds for ESP32-C3, merges the bootloader, partition table, and application, verifies their offsets and bytes, and copies only the verified full image to `build/`.

Use the complete local gate when ESP-IDF 5.5.3 is active:

```bash
./tools/validate.sh
```

## CI and release

- `.github/workflows/static-checks.yml` runs repository, host, and workflow checks on every pull request and `main` push.
- `.github/workflows/firmware-checks.yml` builds on changes to `CMakeLists.txt`, `components/**`, `dependencies.lock`, `main/**`, `partitions.csv`, `sdkconfig.defaults`, `tools/validate.sh`, `tools/verify_firmware.py`, or the workflow itself; it also supports manual dispatch and retains the verified image for seven days.
- `.github/workflows/build-firmware.yml` builds the same image for manual dispatch or a tag and creates a GitHub Release only for a tag.

All workflows call `tools/validate.sh`, use ESP-IDF 5.5.3 for firmware, pin Actions to full commit SHAs, and grant write permission only to the tag release job. If local and CI behavior differs, fix the shared script or environment rather than duplicating commands in YAML.

## Device validation

A successful build is not hardware acceptance. A change under `components/bsp/`, `main/demo_*.c`, `sdkconfig.defaults`, or `partitions.csv` runs every hardware-guide acceptance row for the subsystem it changes. If no board is connected, report those rows under `Unverified` and set `Device tests: NOT RUN`. Report `Build`, `Host tests`, `Device tests`, and `Unverified` separately.
