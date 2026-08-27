<p align="right">
  <strong>简体中文</strong> · <a href="environment-setup.md">English</a>
</p>

# AI Agent 环境引导

本文用于让 AI agent 在全新 checkout 中完成环境搭建，不依赖特定开发者的 shell 函数、绝对路径、IDE 或预装 ESP-IDF。项目要求 ESP32-C3 对应的 ESP-IDF 5.5.3。

## Agent 执行约束

安装任何内容前：

1. 已有 checkout 中先运行 `git status --short --branch`，保留全部用户修改。
2. 检测操作系统和架构；下载新环境前先检查是否已有激活的 ESP-IDF。
3. 仅当 `idf.py --version` 输出 `ESP-IDF v5.5.3` 时复用已有安装。其他版本应并行安装，不得覆盖。
4. 使用 `sudo`、安装系统包、写仓库外目录、修改用户组或通过受限网络下载前，先请求用户授权。
5. 未经用户明确授权，不得修改 shell 启动文件、全局 Git 配置、代理、证书校验或包管理器配置。
6. 不得把机器专用的 shell alias 当作必要命令。
7. 优先使用官方上游和乐鑫运营的下载服务，不得静默切换到未经验证的镜像。

先执行只读探测：

```bash
uname -s
uname -m
command -v idf.py || true
idf.py --version 2>/dev/null || true
printf 'IDF_PATH=%s\n' "${IDF_PATH:-}"
git status --short --branch
```

若正确环境已经激活，直接进入[初始化 checkout](#初始化-checkout)。

## 选择下载线路

默认使用国际线路。用户明确要求，或 GitHub/Registry 直连不可用、过慢时，使用中国大陆线路。镜像变量只作用于当前终端或单条命令。

| 下载内容 | 国际默认线路 | 中国大陆官方线路 |
| --- | --- | --- |
| ESP-IDF 源码 | GitHub `espressif/esp-idf` | 乐鑫 Gitee 镜像及 `esp-gitee-tools` |
| 编译器和工具归档 | GitHub Release Assets | `dl.espressif.cn/github_assets` |
| Managed Components | ESP Component Registry 默认存储 | `components-file.espressif.cn` |
| 当前项目仓库 | 用户提供的 URL | 用户提供的同仓库镜像 |

不得为当前项目编造镜像地址。用户提供的仓库 URL 不可访问时，应询问已授权的镜像或归档地址。

## 安装主机依赖

### Ubuntu 和 Debian

获得系统修改授权后：

```bash
sudo apt-get update
sudo apt-get install -y git wget curl flex bison gperf python3 \
    python3-pip python3-venv cmake ninja-build ccache libffi-dev \
    libssl-dev dfu-util libusb-1.0-0 build-essential
```

### Fedora 及相关发行版

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

先安装 Xcode Command Line Tools 和 Homebrew，再执行：

```bash
xcode-select --install
brew install cmake ninja ccache dfu-util libusb python
```

`xcode-select --install` 和 Homebrew 安装均属于主机级修改且可能需要交互，AI 不得未经用户授权直接执行。

### Windows 与 WSL2

原生 Windows 使用乐鑫官方 ESP-IDF Tools Installer，并选择 ESP-IDF v5.5.3。后续命令应在安装器生成的 ESP-IDF PowerShell 或 Command Prompt 中执行，不得把 POSIX 激活命令机械翻译为 PowerShell。

WSL2 可以执行 Linux 编译流程。烧录和监视需要把 USB 设备转发进 WSL；否则在 WSL 编译，在已激活 ESP-IDF 的原生 Windows 终端烧录。

## 安装 ESP-IDF 5.5.3

安装位置应位于当前仓库之外，路径不得包含空格。以下变量只服务本任务，并允许用户覆盖：

```bash
export AI_PASSPORT_IDF_ROOT="${AI_PASSPORT_IDF_ROOT:-${HOME}/esp/esp-idf-v5.5.3}"
```

### 国际线路

```bash
mkdir -p "$(dirname "${AI_PASSPORT_IDF_ROOT}")"
git clone --branch v5.5.3 --recursive \
    https://github.com/espressif/esp-idf.git \
    "${AI_PASSPORT_IDF_ROOT}"
"${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

clone 中断时应修复原 checkout，不要从头重复下载：

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" submodule update --init --recursive
"${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

### 中国大陆线路

以下线路使用乐鑫运营的仓库和下载端点，不修改全局 Git 或 pip 配置：

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

Gitee 辅助工具报告下载中断时，对同一个 checkout 重新执行子模块命令。不得混用不同 ESP-IDF 版本的部分子模块。

## 激活并核验 ESP-IDF

激活只影响当前 shell，是个人化 alias 的通用替代方案：

```bash
source "${AI_PASSPORT_IDF_ROOT}/export.sh"
idf.py --version
python --version
printf 'IDF_PATH=%s\n' "${IDF_PATH}"
```

版本不是严格的 `ESP-IDF v5.5.3` 时必须停止，不得用其他版本生成项目配置。

中国大陆环境可在当前终端临时加速 Managed Component 归档下载：

```bash
export IDF_COMPONENT_STORAGE_URL="https://components-file.espressif.cn"
```

它只改变组件文件存储端点；版本选择仍由 `components/bsp/idf_component.yml` 和已提交的 `dependencies.lock` 决定。

## 获取项目

若 agent 只拿到仓库地址、尚无 checkout：

```bash
git clone <repository-url> ai-passport
cd ai-passport
git status --short --branch
```

必须原样使用用户提供的 URL，不得把凭证嵌入 URL、打印 token，或把认证信息写入仓库文件。

## 初始化 checkout

在仓库根目录首次编译时，优先运行固件门禁。它会生成并验证用于交付和烧录的
合并固件：

```bash
./tools/validate.sh --firmware
```

仅在需要增量开发编译时使用 `idf.py set-target esp32c3` 和 `idf.py build`。
已有工作区运行 `set-target` 前，应检查并保留有意设置的本地配置，因为它可能把
已有、被忽略的 `sdkconfig` 重命名为 `sdkconfig.old`。

`idf.py fullclean` 只删除构建输出，不能让已有 `sdkconfig` 完整同步新 defaults。

首次构建会把 `dependencies.lock` 锁定的版本下载到 `managed_components/`。不得编辑这个生成目录；普通构建不应留下无法解释的 `dependencies.lock` diff。

核对基线配置：

```bash
grep -E 'CONFIG_IDF_TARGET|CONFIG_ESPTOOLPY_FLASHSIZE|CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG|CONFIG_SPIRAM' sdkconfig
```

预期为 ESP32-C3、8 MB Flash、USB Serial/JTAG 控制台且无 PSRAM。

## 运行仓库门禁

先运行静态门禁，再运行固件门禁：

```bash
./tools/validate.sh --static
./tools/validate.sh --firmware
./tools/validate.sh
```

静态门禁需要 Python 3、C 编译器、`curl`、`tar` 和 SHA-256 工具；未安装
`actionlint` 时，会把带固定校验和的版本下载到 `/tmp`。固件门禁是默认优先的
编译方式，它使用隔离的临时构建，并生成经过验证的 `0x0` 镜像：

```text
build/FoloToy-AI-Passport-full.bin
```

对于已经获得 Docker 使用授权、只需编译的 agent，乐鑫官方镜像可以替代主机安装：

```bash
docker run --rm \
    -v "${PWD}:/project" \
    -w /project \
    -e IDF_GIT_SAFE_DIR=/project \
    espressif/idf:v5.5.3 \
    ./tools/validate.sh --firmware
```

Docker 本身不会提供安全的 USB 烧录访问，挂载 checkout 后容器也可以写生成文件。拉取镜像或使用 Docker 前必须获得授权。

## 烧录与监视

编译不要求设备，硬件验证必须访问设备。使用支持数据传输的 USB 线，发现实际端口，不得在仓库中硬编码：

```bash
ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null
```

Linux 串口访问通常要求用户属于 `dialout`（部分发行版为 `uucp`）。修改用户组前必须获得授权，且修改后需要重新登录：

```bash
sudo usermod -aG dialout "${USER}"
```

烧录前关闭 WebSerial 页面和其他串口监视器。不要长期以 root 身份运行日常开发流程。使用 `Ctrl+]` 退出 ESP-IDF monitor。

优先从 `0x0` 烧录经过验证的合并镜像：

```bash
python -m esptool --chip esp32c3 -p <port> -b 460800 \
    write-flash 0x0 build/FoloToy-AI-Passport-full.bin
idf.py -p <port> monitor
```

普通的 `build/FoloToy-AI-Passport.bin` 只是 app，只能位于 `0x10000`，不得烧到
`0x0`。`idf.py flash` 只用于明确需要的增量开发烧录，不作为默认交付或验收方式。

## 故障处理

| 症状 | 处理 |
| --- | --- |
| 找不到 `idf.py` | 激活所选安装的 `export.sh`，不得猜测个人 alias。 |
| ESP-IDF 版本错误 | 停止并并行激活/安装 v5.5.3。 |
| GitHub 源码或工具下载慢 | 切换到本文的乐鑫中国大陆线路。 |
| 中国大陆组件下载慢 | 在当前终端设置 `IDF_COMPONENT_STORAGE_URL`。 |
| 组件下载失败 | 检查网络、代理、DNS 和证书，不得通过关闭 TLS 校验绕过。 |
| 配置缺少已跟踪 defaults | 保留有意配置，再执行 `idf.py set-target esp32c3`。 |
| 构建来自其他 ESP-IDF | 激活 v5.5.3，执行 `idf.py fullclean`，再 set-target/build。 |
| 串口不存在 | 检查线材、枚举、主机/虚拟机 USB 转发和供电。 |
| 串口被占用 | 关闭 WebSerial、VS Code monitor、`idf.py monitor` 和其他串口客户端。 |
| 串口 permission denied | 把用户加入平台串口组并重新登录。 |

## 完成报告

AI agent 必须分别报告环境、构建与硬件结果：

```text
Environment: PASS / FAIL（OS、架构、ESP-IDF 版本、下载线路）
Build: PASS / FAIL / NOT RUN
Host tests: PASS / FAIL / NOT RUN
Device tests: PASS / FAIL / NOT RUN
Unverified: 仍需 USB、板卡、仪器或用户确认的事项
```

官方参考：

- [ESP-IDF v5.5 工具链安装](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/get-started/linux-macos-setup.html)
- [ESP-IDF v5.5 项目初始化](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/get-started/linux-macos-start-project.html)
- [乐鑫下载镜像配置](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c3/api-guides/tools/idf-tools.html)
- [IDF Component Manager 配置](https://docs.espressif.com/projects/idf-component-manager/en/latest/use/how_to_configuration.html)
- [乐鑫 Gitee 工具](https://gitee.com/EspressifSystems/esp-gitee-tools)
- [ESP-IDF Windows 安装](https://docs.espressif.com/projects/esp-idf/en/release-v5.5/esp32c3/get-started/windows-setup.html)
