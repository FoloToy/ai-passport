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
| ESP-IDF source | GitHub `espressif/esp-idf` | Espressif Gitee mirror plus `esp-gitee-tools` |
| Compiler and tool archives | GitHub release assets | `dl.espressif.cn/github_assets` |
| Managed Components | ESP Component Registry default storage | `components-file.espressif.cn` |
| Project repository | URL supplied by the user | A user-supplied mirror of the same repository |

Do not invent a mirror URL for this project. If the supplied repository URL is
unreachable, ask the user for an authorized mirror or archive.

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

The following route uses repositories and download endpoints operated by
Espressif. It does not change global Git or pip configuration:

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
not installed. The firmware gate is the preferred build path. It uses an
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
