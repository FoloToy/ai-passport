<p align="right">
  <a href="environment-setup.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Environment Bootstrap for AI Agents

This document lets an AI agent bootstrap a clean checkout without relying on a
developer-specific shell function, absolute path, IDE, or preinstalled ESP-IDF.
The required baseline is ESP-IDF 5.5.3 for ESP32-C3.

## Agent contract

Before installing anything:

1. Run `git status --short --branch` in an existing checkout and preserve all
   user changes.
2. Detect the operating system and architecture. Check for an already activated
   ESP-IDF before downloading another copy.
3. Reuse an existing installation only when `idf.py --version` reports
   `ESP-IDF v5.5.3`. Install other versions side by side; do not replace them.
4. Ask for approval before using `sudo`, installing system packages, writing
   outside the repository, changing group membership, or downloading through a
   restricted network.
5. Do not edit shell startup files, global Git configuration, proxy settings,
   certificate verification, or package-manager configuration without explicit
   user authorization.
6. Never use a machine-specific shell alias as a required command.
7. Prefer official upstream and Espressif-operated download services. Do not
   silently fall back to an unverified mirror.

Start with read-only discovery:

```bash
uname -s
uname -m
command -v idf.py || true
idf.py --version 2>/dev/null || true
printf 'IDF_PATH=%s\n' "${IDF_PATH:-}"
git status --short --branch
```

If the correct environment is already active, continue at
[Initialize the checkout](#initialize-the-checkout).

## Choose a download route

Use the international route by default. Use the mainland China route when the
user requests it or direct GitHub/registry downloads are unavailable or too
slow. Keep mirror variables scoped to the current terminal or command.

| Download | International default | Mainland China official route |
| --- | --- | --- |
| ESP-IDF source | GitHub `espressif/esp-idf` | Espressif Git service mirror (`git.espressif.com.cn`), or Gitee mirror plus `esp-gitee-tools` |
| Compiler and tool archives | GitHub release assets | `dl.espressif.cn/github_assets` |
| Managed Components | ESP Component Registry default storage | `components-file.espressif.cn` |
| Project repository | URL supplied by the user | A user-supplied mirror of the same repository |

Do not invent a mirror URL for this project. If the supplied repository URL is
unreachable, ask the user for an authorized mirror or archive.

The Espressif Git service mirror hosts the full `esp-idf` history and every
v5.5.3 submodule repository (`espressif/esp32-wifi-lib`, `espressif/esp-lwip`,
`ThrowTheSwitch/CMock`, and so on). Because the v5.5.3 `.gitmodules` entries are
relative URLs, cloning the parent repository from the mirror resolves every
submodule to the same mirror automatically; no separate submodule rewrite is
needed. Cloning from the mirror leaves a working checkout identical to the
GitHub clone: verify the tag commit matches
`https://github.com/espressif/esp-idf/releases/tag/v5.5.3` (currently
`b31fcc7a314a44ad992b58f589f7d1d8a4fadff6`).

Legacy mirror installers sometimes write a global per-repository `insteadOf`
rewrite for Jihulab (`jihulab.com/esp-mirror/...`, or a
`git config --global url."https://jihulab.com/esp-mirror/".insteadOf` switch).
A per-repository entry is more specific than any bulk switch and silently
redirects submodules to a source that returns 404 or timeouts. When a submodule
fetch fails this way, inspect and clean the residue after user approval instead
of retrying blindly:

```bash
git config --global --get-regexp 'url\..*jihulab'
# Cleaning requires user approval: unset each entry individually; removing only
# the bulk switch leaves per-repository entries that still redirect submodules.
```

## Install host prerequisites

### Ubuntu and Debian

After approval for system changes:

```bash
sudo apt-get update
sudo apt-get install -y git wget curl flex bison gperf python3 \
    python3-pip python3-venv cmake ninja-build ccache libffi-dev \
    libssl-dev dfu-util libusb-1.0-0 build-essential
```

### Fedora and related distributions

```bash
sudo dnf install -y git wget curl flex bison gperf python3 cmake \
    ninja-build ccache libffi-devel openssl-devel dfu-util libusb1-devel \
    gcc gcc-c++ make
```

### Arch Linux

```bash
sudo pacman -S --needed base-devel git wget curl flex bison gperf python \
    cmake ninja ccache dfu-util libusb
```

### macOS

Install Xcode Command Line Tools and Homebrew first, then:

```bash
xcode-select --install
brew install cmake ninja ccache dfu-util libusb python
```

Do not launch `xcode-select --install` or install Homebrew without user approval;
both are host-level changes and may require interaction.

### Windows and WSL2

For native Windows, use Espressif's official ESP-IDF Tools Installer and select
ESP-IDF v5.5.3. Run later commands in the installer-created ESP-IDF PowerShell or
Command Prompt. Do not translate POSIX activation commands literally into
PowerShell.

WSL2 can run the Linux build flow. USB flashing and monitoring require a device
forwarded into WSL; otherwise build in WSL and flash from an activated native
Windows ESP-IDF terminal.

## Install ESP-IDF 5.5.3

Use a location outside this repository and without spaces. The variable below
is task-specific and can be overridden by the user:

```bash
export AI_PASSPORT_IDF_ROOT="${AI_PASSPORT_IDF_ROOT:-${HOME}/esp/esp-idf-v5.5.3}"
```

### International route

```bash
mkdir -p "$(dirname "${AI_PASSPORT_IDF_ROOT}")"
git clone --branch v5.5.3 --recursive \
    https://github.com/espressif/esp-idf.git \
    "${AI_PASSPORT_IDF_ROOT}"
"${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

If cloning was interrupted, repair the checkout instead of starting over:

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" submodule update --init --recursive
"${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

### Mainland China route

The following routes use repositories and download endpoints operated by
Espressif. They do not change global Git or pip configuration.

**Espressif Git service mirror (preferred).** Clone from the mirror URL
directly — `origin` becomes the mirror URL, so every relative submodule URL
inside `esp-idf` (`../../espressif/...`) resolves to the same mirror without a
rewrite and without global Git configuration:

```bash
mkdir -p "$(dirname "${AI_PASSPORT_IDF_ROOT}")"
git clone --branch v5.5.3 --recursive \
    https://git.espressif.com.cn/espressif/esp-idf.git \
    "${AI_PASSPORT_IDF_ROOT}"
IDF_GITHUB_ASSETS=dl.espressif.cn/github_assets \
    "${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

The mirror has published the same v5.5.3 tag commit as GitHub
(`b31fcc7a314a44ad992b58f589f7d1d8a4fadff6`), and it hosts the submodule
repositories themselves, so the checkout is identical to the GitHub clone.

**Gitee mirror plus esp-gitee-tools (fallback).**

```bash
export AI_PASSPORT_GITEE_TOOLS_ROOT="${AI_PASSPORT_GITEE_TOOLS_ROOT:-${HOME}/esp/esp-gitee-tools}"
mkdir -p "$(dirname "${AI_PASSPORT_IDF_ROOT}")"
git clone --branch v5.5.3 \
    https://gitee.com/EspressifSystems/esp-idf.git \
    "${AI_PASSPORT_IDF_ROOT}"
git clone https://gitee.com/EspressifSystems/esp-gitee-tools.git \
    "${AI_PASSPORT_GITEE_TOOLS_ROOT}"
"${AI_PASSPORT_GITEE_TOOLS_ROOT}/submodule-update.sh" \
    "${AI_PASSPORT_IDF_ROOT}"
IDF_GITHUB_ASSETS=dl.espressif.cn/github_assets \
    "${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

If the Gitee helper reports an interrupted download, rerun its submodule command
for the same checkout. Do not mix partial submodules from unrelated ESP-IDF
versions.

### Submodule pulls and long waits

`esp-idf` v5.5.3 bundles more than twenty submodules, and several of them
(`components/esp_wifi/lib`, `components/esp_phy/lib`,
`components/bt/controller/lib_esp32c3_family`) are large prebuilt libraries.
A `git submodule update --init --recursive` can therefore run for minutes with
little output, and agent shells commonly kill it after a fixed wait (for
example a 300000 ms or "exceeding timeout" message) while the process is still
fetching. Check the real state first instead of assuming the mirror failed:

- Submodule fetching in progress is not a failure until the shell reports an
  exit code AND `git status` stays non-clean. Re-run with a longer shell wait
  limit, or run the command in the background until it truly exits.
- An interrupted `git submodule update` leaves `modified:` entries under
  `components/` (for example `components/esp_wifi/lib`). This is a partial
  checkout, not corrupted state.

Repair in place rather than cloning from scratch:

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" submodule update --init --recursive
# if status still lists submodule paths, update each one by path:
git -C "${AI_PASSPORT_IDF_ROOT}" submodule update --init components/<path-from-status>
git -C "${AI_PASSPORT_IDF_ROOT}" status --short
```

Keep the checkout clean before running `install.sh`; `idf.py` ignores
uninitialized submodules differently across versions and a partially updated
tree produces confusing compile errors.

### Large submodule fixes

`components/esp_wifi/lib` resolves to `espressif/esp32-wifi-lib`, a large
prebuilt repository, and a full fetch of it commonly exceeds any reasonable
wait limit. Do not retry the full fetch repeatedly on a short wait; fetch only
the pinned commit instead:

1. Read the exact commit `esp-idf` pins for that path:

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" ls-tree HEAD components/esp_wifi/lib
# 160000 commit <sha>  components/esp_wifi/lib
```

2. Remove the partial directory and fetch that single commit shallowly:

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" rm -rf components/esp_wifi/lib
mkdir -p "${AI_PASSPORT_IDF_ROOT}/components/esp_wifi/lib"
cd "${AI_PASSPORT_IDF_ROOT}/components/esp_wifi/lib"
git init
git remote add origin https://github.com/espressif/esp32-wifi-lib.git
git fetch --depth 1 origin <sha-from-ls-tree>
git checkout FETCH_HEAD
```

3. Let `git submodule update --init` record the checkout, then verify:

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" submodule update --init components/esp_wifi/lib
git -C "${AI_PASSPORT_IDF_ROOT}" status --short
```

Do not use `git submodule update --depth=1 <path>` for these pins: it fetches
only the default-branch tip and fails with
`Unable to find current revision in submodule path` when the pinned commit is
not the tip. For any remaining path that stays dirty after the ordinary repair,
run `git submodule deinit -f <path>` first, then `update --init` again. Always
use the paths printed by the latest `git status`; do not copy example paths.

### Offline archives as a last resort

If every source is unreachable after repeated attempts, the official Espressif
release archive is a complete checkout snapshot — it already contains `.git`
and every v5.5.3 submodule — so unpacking it needs no network or Git operation
after download:

```bash
curl -fL -o /tmp/esp-idf-v5.5.3.zip \
    https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/v5.5.3/esp-idf-v5.5.3.zip
mkdir -p "$(dirname "${AI_PASSPORT_IDF_ROOT}")"
unzip -q /tmp/esp-idf-v5.5.3.zip -d "$(dirname "${AI_PASSPORT_IDF_ROOT}")"
"${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

The archive expands to a `esp-idf-v5.5.3/` directory (about 1.8 GB). Use the
`dl.espressif.cn` host variant if `dl.espressif.com` is slow; both serve the
same file. Validate the expanded tree with the checksum released next to the
archive before reusing it as an installation base.

## Activate and verify ESP-IDF

Activation applies to the current shell only and is the portable replacement
for machine-specific aliases:

```bash
source "${AI_PASSPORT_IDF_ROOT}/export.sh"
idf.py --version
python --version
printf 'IDF_PATH=%s\n' "${IDF_PATH}"
```

Stop if the reported version is not exactly `ESP-IDF v5.5.3`. Do not generate
project configuration with another version.

Activation is shell-scoped: run `export.sh` with a leading dot (or `source`),
never as `./export.sh`, which spawns a subshell and leaves the current shell
unconfigured (`IDF_PATH is not set` or `idf.py: command not found` afterward).
If a custom `IDF_TOOLS_PATH` was used during install, set it again before every
`export.sh` call — the script does not remember it from an earlier session.

### Toolchain install failures

For a checksum mismatch or corrupted toolchain download, first rerun the
installer from the selected ESP-IDF 5.5.3 checkout, keeping the same
`IDF_TOOLS_PATH` and download-route settings. For example, on Linux/macOS:

```bash
"${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

If using the mainland China route, apply `IDF_GITHUB_ASSETS` to this invocation
as shown in [Choose a download route](#choose-a-download-route). The
[ESP-IDF 5.5.3 installer](https://github.com/espressif/esp-idf/blob/v5.5.3/tools/idf_tools.py)
validates existing archives and replaces the failed archive rather than
clearing all cached downloads.

The download cache is `dist` under the effective `IDF_TOOLS_PATH`. Without an
override, this is normally `~/.espressif/dist` on Linux/macOS or
`%USERPROFILE%\.espressif\dist` on Windows. Do not clear the whole directory:
it can contain valid offline archives shared by other ESP-IDF versions. If a
manual intervention is still needed, identify the exact failed archive from
the log and the actual tools path, obtain approval, and move only that file
aside for recovery before retrying. On Windows, retry the official installer
or rerun `install.bat esp32c3` in an ESP-IDF Command Prompt from the selected
checkout, with the same custom tools path and route settings in that terminal.

On macOS, `[SSL: CERTIFICATE_VERIFY_FAILED]` means certificate verification
failed; it does not identify one universal fix. Confirm which Python
interpreter the installer uses and how it was installed, then check that
interpreter's trust store and any approved proxy. The
[`Install Certificates.command` helper](https://docs.python.org/3.13/using/mac.html#installation-steps)
belongs to the python.org macOS installer: use it only for the matching Python
installation that provides it. Homebrew and other Python distributions may
not include it; follow their certificate setup instead. Obtain approval for
trust-store changes and never disable TLS verification.

On Apple Silicon, `tool riscv32-esp-elf has no installed versions` can mean the
tool has not been installed or the tools path is wrong; it does not establish
an architecture mismatch. Verify `IDF_TOOLS_PATH` and rerun the ESP32-C3
installer first. For `bad CPU type in executable`, check the host, active
shell/Python, and failing binary architectures. ESP-IDF 5.5.3 provides a
[native macOS ARM64 ESP32-C3 toolchain](https://github.com/espressif/esp-idf/blob/v5.5.3/tools/tools.json);
prefer it. Only consider Rosetta after confirming that a required executable
is x86-64-only, and obtain approval before installing it. It is not a routine
ESP32-C3 prerequisite.

For mainland China, optionally accelerate Managed Component archives in the
current terminal:

```bash
export IDF_COMPONENT_STORAGE_URL="https://components-file.espressif.cn"
```

This changes only the component file-storage endpoint. Version selection still
comes from `components/bsp/idf_component.yml` and the tracked
`dependencies.lock`.

## Obtain the project

If the agent was given a repository URL but has no checkout yet:

```bash
git clone <repository-url> ai-passport
cd ai-passport
git status --short --branch
```

Use the user-supplied URL verbatim. Do not embed credentials in it, print tokens,
or persist authentication data in repository files.

## Initialize the checkout

From the repository root, prefer the firmware gate for the first build. It
creates and verifies the merged image intended for delivery and flashing:

```bash
./tools/validate.sh --firmware
```

Use `idf.py set-target esp32c3` and `idf.py build` only when an incremental
development build is useful. Before running `set-target` in an established
workspace, inspect and preserve intentional local configuration because it may
rename an existing ignored `sdkconfig` to `sdkconfig.old`.

`idf.py fullclean` removes build output but does not fully synchronize an
existing `sdkconfig` with changed defaults.

The first build downloads the versions pinned by `dependencies.lock` into
`managed_components/`. Never edit that generated directory. A normal build must
not leave an unexplained `dependencies.lock` diff.

Confirm the baseline configuration:

```bash
grep -E 'CONFIG_IDF_TARGET|CONFIG_ESPTOOLPY_FLASHSIZE|CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG|CONFIG_SPIRAM' sdkconfig
```

Expected: ESP32-C3, 8 MB Flash, USB Serial/JTAG console, and no PSRAM.

## Run the repository gate

Run the static gate first, then the firmware gate:

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

The static gate requires Python 3, a C compiler, `curl`, `tar`, and a SHA-256
tool. It downloads a checksum-pinned `actionlint` release into `/tmp` when one is
not installed. On a machine without network access, set `ACTIONLINT=offline` to
use only an installed or cached binary and report `actionlint: SKIPPED` instead
of downloading; `ACTIONLINT=/path/to/actionlint` selects a specific binary. The
firmware gate is the preferred build path. It uses an
isolated temporary build and produces the verified `0x0` image at:

```text
build/FoloToy-AI-Passport-full.bin
```

For build-only agents with Docker already authorized, the official Espressif
image is an alternative to a host installation:

```bash
docker run --rm \
    -v "${PWD}:/project" \
    -w /project \
    -e IDF_GIT_SAFE_DIR=/project \
    espressif/idf:v5.5.3 \
    ./tools/validate.sh --firmware
```

Docker does not by itself provide safe USB access for flashing, and mounting the
checkout allows the container to write generated files. Obtain approval before
pulling the image or using Docker.

## Flash and monitor

Device access is optional for compilation but required for hardware validation.
Use a data-capable USB cable and discover the actual port; never hardcode it in
the repository:

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

On Linux, serial access commonly requires membership in `dialout` (or `uucp` on
some distributions). Changing group membership requires user approval and a new
login session:

```bash
sudo usermod -aG dialout "${USER}"
```

Close WebSerial pages and other serial monitors before flashing. Do not run the
normal development flow permanently as root. Exit the ESP-IDF monitor with
`Ctrl+]`.

Prefer flashing the verified merged image from offset `0x0`:

```bash
python -m esptool --chip esp32c3 -p <port> -b 460800 \
    write-flash 0x0 build/FoloToy-AI-Passport-full.bin
idf.py -p <port> monitor
```

The ordinary `build/FoloToy-AI-Passport.bin` is application-only and belongs at
`0x10000`; it must not be written to `0x0`. Use `idf.py flash` only for an
intentional incremental development flash, not as the default delivery or
acceptance path.

## Failure handling

| Symptom | Action |
| --- | --- |
| `idf.py` not found | Activate the selected installation's `export.sh`; do not guess a private alias. |
| Wrong ESP-IDF version | Stop and activate/install v5.5.3 side by side. |
| GitHub source or asset download is slow | Switch to the documented Espressif mainland China route. |
| Submodule fetch times out or the shell reports an exceeded wait | Confirm `git status` first; if still fetching, rerun with a longer wait limit or in the background, then repair with `git submodule update --init --recursive`. |
| Submodule fetch 404s or retries against a stale mirror | Inspect `git config --global --get-regexp 'url\..*jihulab'`; obtain user approval to clean residue before retrying. |
| Component download is slow in China | Set `IDF_COMPONENT_STORAGE_URL` for the current terminal. |
| Component download fails | Check network, proxy, DNS, and certificates; never disable TLS verification as a shortcut. |
| Configuration misses tracked defaults | Preserve intentional settings, then rerun `idf.py set-target esp32c3`. |
| Build came from another ESP-IDF | Activate v5.5.3, run `idf.py fullclean`, then `idf.py set-target esp32c3`. |
| Serial port is missing | Check cable, enumeration, host/VM USB forwarding, and power. |
| Serial port is busy | Close WebSerial, VS Code monitor, `idf.py monitor`, and other serial clients. |
| Permission denied on serial port | Add the user to the platform serial group and log in again. |

## Completion report

An AI agent must report environment and hardware results separately:

```text
Environment: PASS / FAIL (OS, architecture, ESP-IDF version, download route)
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: remaining USB, board, instrument, or user checks
```

Official references:

- [ESP-IDF v5.5 toolchain setup](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/get-started/linux-macos-setup.html)
- [ESP-IDF v5.5 project setup](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/get-started/linux-macos-start-project.html)
- [Espressif download mirror configuration](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c3/api-guides/tools/idf-tools.html)
- [IDF Component Manager configuration](https://docs.espressif.com/projects/idf-component-manager/en/latest/use/how_to_configuration.html)
- [Espressif Gitee tools](https://gitee.com/EspressifSystems/esp-gitee-tools)
- [ESP-IDF Windows setup](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/get-started/windows-setup.html)
