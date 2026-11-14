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
| ESP-IDF 源码 | GitHub `espressif/esp-idf` | 乐鑫 Git 服务镜像（`git.espressif.com.cn`），或 Gitee 镜像及 `esp-gitee-tools` |
| 编译器和工具归档 | GitHub Release Assets | `dl.espressif.cn/github_assets` |
| Managed Components | ESP Component Registry 默认存储 | `components-file.espressif.cn` |
| 当前项目仓库 | 用户提供的 URL | 用户提供的同仓库镜像 |

不得为当前项目编造镜像地址。用户提供的仓库 URL 不可访问时，应询问已授权的镜像或归档地址。

乐鑫 Git 服务镜像收录了完整 `esp-idf` 历史以及 v5.5.3 的全部子模块仓库
（`espressif/esp32-wifi-lib`、`espressif/esp-lwip`、`ThrowTheSwitch/CMock` 等）。
由于 v5.5.3 的 `.gitmodules` 条目全部是相对 URL，从该镜像克隆父仓库后，每个
子模块会自动解析到同一镜像，无需单独的镜像改写步骤。从镜像克隆得到的
checkout 与 GitHub 克隆完全一致：请核对 tag commit 与
`https://github.com/espressif/esp-idf/releases/tag/v5.5.3` 相同（当前为
`b31fcc7a314a44ad992b58f589f7d1d8a4fadff6`）。

旧版镜像安装脚本有时会在全局 Git 配置中写入按仓库的 `insteadOf` 重定向
（`jihulab.com/esp-mirror/...`，或
`git config --global url."https://jihulab.com/esp-mirror/".insteadOf` 总开关）。
按仓库的条目比任何总开关更具体，会把子模块静默重定向到返回 404 或超时的
源。当子模块拉取因此失败时，先检查并在用户授权下清理残留，而不是盲目重试：

```bash
git config --global --get-regexp 'url\..*jihulab'
# 清理需要用户授权：逐条 unset；只删总开关会留下仍会劫持子模块的按仓库条目。
```

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

以下线路使用乐鑫运营的仓库和下载端点，不修改全局 Git 或 pip 配置。

**乐鑫 Git 服务镜像（首选）。** 直接克隆镜像 URL —— `origin` 就是镜像地址，
`esp-idf` 内部的相对子模块 URL（`../../espressif/...`）会全部解析到同一镜像，
既不需要改写规则，也不需要全局 Git 配置：

```bash
mkdir -p "$(dirname "${AI_PASSPORT_IDF_ROOT}")"
git clone --branch v5.5.3 --recursive \
    https://git.espressif.com.cn/espressif/esp-idf.git \
    "${AI_PASSPORT_IDF_ROOT}"
IDF_GITHUB_ASSETS=dl.espressif.cn/github_assets \
    "${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

镜像发布了与 GitHub 相同的 v5.5.3 tag commit
（`b31fcc7a314a44ad992b58f589f7d1d8a4fadff6`），并且托管了各子模块仓库本身，
因此得到的 checkout 与 GitHub 克隆完全一致。

**Gitee 镜像及 esp-gitee-tools（备选）。**

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

### 子模块拉取与长等待

`esp-idf` v5.5.3 内含 20 余个子模块，其中
（`components/esp_wifi/lib`、`components/esp_phy/lib`、
`components/bt/controller/lib_esp32c3_family`）是大体积预编译库。
`git submodule update --init --recursive` 可能长时间没有输出，agent 的 shell
也常按固定等待时间把它杀掉（例如 300000 ms 或 "exceeding timeout"），而此时
进程还在继续拉取。先检查真实状态，不要贸然认定镜像失败：

- 只要 shell 还没报告退出码且 `git status` 仍然不干净，子模块拉取就不算失败。
  用更长的 shell 等待时间重试，或后台执行直到真正退出。
- `git submodule update` 被中断后，`components/` 下会出现 `modified:` 条目
  （例如 `components/esp_wifi/lib`）。这是部分 checkout，不是损坏状态。

原地修复，而不是重新克隆：

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" submodule update --init --recursive
# 若 status 仍列出子模块路径，按路径逐个更新：
git -C "${AI_PASSPORT_IDF_ROOT}" submodule update --init components/<status-中的路径>
git -C "${AI_PASSPORT_IDF_ROOT}" status --short
```

运行 `install.sh` 前保持 checkout 干净；各版本 `idf.py` 对未初始化子模块的
处理不同，部分更新的目录树会产生难以定位的编译错误。

### 大仓子模块修复

`components/esp_wifi/lib` 对应 `espressif/esp32-wifi-lib`，是体积很大的
预编译仓库，全量拉取通常超过任何合理等待上限。不要在短等待下反复重试全量
拉取，改为只取钉死的那个 commit：

1. 读取 `esp-idf` 为该路径钉死的 commit：

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" ls-tree HEAD components/esp_wifi/lib
# 160000 commit <sha>  components/esp_wifi/lib
```

2. 删除部分目录，按该 commit 浅取：

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" rm -rf components/esp_wifi/lib
mkdir -p "${AI_PASSPORT_IDF_ROOT}/components/esp_wifi/lib"
cd "${AI_PASSPORT_IDF_ROOT}/components/esp_wifi/lib"
git init
git remote add origin https://github.com/espressif/esp32-wifi-lib.git
git fetch --depth 1 origin <ls-tree-输出的-sha>
git checkout FETCH_HEAD
```

3. 让 `git submodule update --init` 登记该 checkout，再核验：

```bash
git -C "${AI_PASSPORT_IDF_ROOT}" submodule update --init components/esp_wifi/lib
git -C "${AI_PASSPORT_IDF_ROOT}" status --short
```

对这些钉死路径不要使用 `git submodule update --depth=1 <path>`：它只浅拉
默认分支 tip，当钉死的 commit 不是 tip 时会报
`Unable to find current revision in submodule path`。普通修复后仍然 dirty 的
路径，先执行 `git submodule deinit -f <path>` 再 `update --init`。路径一律取
本次 `git status` 打印的实际值，不要照抄示例路径。

### 离线压缩包兜底

重复尝试后所有源都不可达时，乐鑫官方 release 归档是一次完整 checkout 快照
——已包含 `.git` 与 v5.5.3 的全部子模块——下载后解压即用，不需要任何网络或
Git 操作：

```bash
curl -fL -o /tmp/esp-idf-v5.5.3.zip \
    https://dl.espressif.com/github_assets/espressif/esp-idf/releases/download/v5.5.3/esp-idf-v5.5.3.zip
mkdir -p "$(dirname "${AI_PASSPORT_IDF_ROOT}")"
unzip -q /tmp/esp-idf-v5.5.3.zip -d "$(dirname "${AI_PASSPORT_IDF_ROOT}")"
"${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

归档解压后为 `esp-idf-v5.5.3/` 目录（约 1.8 GB）。若 `dl.espressif.com` 较慢，
可使用 `dl.espressif.cn` 主机变体，两者提供同一文件。复用该目录作为安装基础
前，用归档旁发布的校验和验证解压结果。

## 激活并核验 ESP-IDF

激活只影响当前 shell，是个人化 alias 的通用替代方案：

```bash
source "${AI_PASSPORT_IDF_ROOT}/export.sh"
idf.py --version
python --version
printf 'IDF_PATH=%s\n' "${IDF_PATH}"
```

版本不是严格的 `ESP-IDF v5.5.3` 时必须停止，不得用其他版本生成项目配置。

激活只对当前 shell 生效：运行 `export.sh` 必须用带点的形式（或 `source`），
不能写成 `./export.sh`——那会在子 shell 中执行，当前 shell 不会配置成功
（之后报 `IDF_PATH is not set` 或 `idf.py: command not found`）。安装时使用过
自定义 `IDF_TOOLS_PATH` 的，每次 `export.sh` 前都要重新设置——脚本不会记住
上一个 session 的值。

### 工具链安装失败

工具链下载出现校验和不匹配或归档损坏时，先从选定的 ESP-IDF 5.5.3 checkout
重新运行安装器，保持相同的 `IDF_TOOLS_PATH` 和下载路线设置。例如在
Linux/macOS 上：

```bash
"${AI_PASSPORT_IDF_ROOT}/install.sh" esp32c3
```

使用中国大陆路线时，按[选择下载路线](#选择下载路线)中的示例，把
`IDF_GITHUB_ASSETS` 应用于这次调用。
[ESP-IDF 5.5.3 安装器](https://github.com/espressif/esp-idf/blob/v5.5.3/tools/idf_tools.py)
会校验已有归档并替换失败的归档，而不是清空全部下载缓存。

下载缓存位于实际 `IDF_TOOLS_PATH` 下的 `dist` 目录。未覆盖路径时，通常为
Linux/macOS 的 `~/.espressif/dist` 或 Windows 的
`%USERPROFILE%\.espressif\dist`。不要清空整个目录，其中可能含其他 ESP-IDF
版本共用的有效离线归档。若仍需手动处理，先根据日志和实际工具路径确认
具体失败的归档，取得批准后仅把该文件移到可恢复的位置，再重试。
Windows 上重试官方安装器，或在 ESP-IDF Command Prompt 中进入选定 checkout
后重新运行 `install.bat esp32c3`；在该终端中保留相同的自定义工具路径和下载
路线设置。

macOS 上的 `[SSL: CERTIFICATE_VERIFY_FAILED]` 只表示证书校验失败，并不能
确定唯一修复方式。先确认安装器实际使用的 Python 解释器及其安装来源，
再检查该解释器的信任证书和已批准使用的代理。
[`Install Certificates.command` 辅助脚本](https://docs.python.org/3.13/using/mac.html#installation-steps)
属于 python.org 的 macOS 安装器流程，只对提供该脚本的对应 Python 安装使用。
Homebrew 等其他 Python 发行方式不一定提供它，应按其证书配置流程处理。
更改信任证书前需取得批准，禁止通过关闭 TLS 校验绕过错误。

Apple Silicon 上的 `tool riscv32-esp-elf has no installed versions` 可能只是
工具未安装或工具路径错误，不能据此判断架构不匹配。先核对 `IDF_TOOLS_PATH`
并重新运行 ESP32-C3 安装器。若出现 `bad CPU type in executable`，再检查
主机、当前 shell／Python 和失败二进制的架构。ESP-IDF 5.5.3 提供
[原生 macOS ARM64 ESP32-C3 工具链](https://github.com/espressif/esp-idf/blob/v5.5.3/tools/tools.json)，
应优先使用。只有确认必须使用仅支持 x86-64 的可执行程序后，才考虑 Rosetta，
且安装前必须取得批准；它不是 ESP32-C3 的常规前置条件。

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
`actionlint` 时，会把带固定校验和的版本下载到 `/tmp`。无法联网的机器可设置
`ACTIONLINT=offline`：只使用已安装或已缓存的版本，没有时报告
`actionlint: SKIPPED` 而不下载；`ACTIONLINT=/path/to/actionlint` 可指定具体可执行
文件。固件门禁是默认优先的
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
| 子模块拉取超时或 shell 报告等待超限 | 先确认 `git status`；若仍在拉取，用更长等待时间或后台重试，再用 `git submodule update --init --recursive` 修复。 |
| 子模块拉取 404 或反复回到过期镜像 | 检查 `git config --global --get-regexp 'url\..*jihulab'`；清理残留前先获得用户授权。 |
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
