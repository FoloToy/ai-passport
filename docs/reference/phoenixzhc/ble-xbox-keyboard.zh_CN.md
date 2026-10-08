<p align="right">
  <strong>简体中文</strong> · <a href="ble-xbox-keyboard.md">English</a>
</p>

# AI Passport BLE Xbox 手柄与键盘接入经验

在 ESP32-C3 上接入蓝牙输入设备，必须分别验证发现、配对、HID 服务、报告解析和应用输入。能扫描到名称、链路已加密、手柄灯常亮，都不能单独证明按键已经正确进入应用。

本文整理自 [FoloToy-GameBoy][gb] 与 [FoloToy-FC][fc]，面向使用 ESP-IDF、Bluepad32 和 BTstack 开发 AI Passport 应用的开发者。重点是实际遇到的故障、修复方法和可复用的测试；它不是上游默认固件的功能承诺，也不是所有蓝牙设备的兼容名单。

## 1 适用环境和已知实测结果

| 项目 | 本文对应范围 |
| --- | --- |
| 硬件 | AI Passport，ESP32-C3，8 MiB Flash，无 PSRAM |
| 开发环境 | ESP-IDF 5.5.3 |
| GameBoy 源码 | `f221fe83fc40ec07f37f93bb19ce3d74879e8718`，版本 1.4 |
| FC 源码 | `d1206fae4feb650a1c7d2a7a211cda4205ce3381`，版本 1.0 |
| Bluepad32 来源 | `e9b755faabc240585da42e6d26164bb2cdd064d3`，包含项目本地修补 |
| BTstack 来源 | `5d4d8cc7b1d35a90bbd6d5ffd2d3050b2bfc861c`，包含项目本地修补 |
| 同时连接数量 | 一个输入设备；手柄、键盘二选一 |

两项目的 `gamepad.c`、`gamepad_discovery.c`、`gb_input.c`，以及下文涉及的 BLE 发现、HID 分类、初始化、键盘解析和 SMP 修补共九个关键文件，在上述提交中内容一致。共同实现可以复用，但一个项目的设备测试不能自动代替另一个项目的设备测试。

| 设备或测试 | 已确认结果 | 适用边界 |
| --- | --- | --- |
| Xbox Wireless Controller 1914，手柄固件 5.22.16.0 | GameBoy 历史真机记录确认绑定、HID 输入、重启后密钥重加密，以及一次游戏中断开后自动重连 | 不代表所有 Xbox 型号、后续每个固件版本或多次重连耐久性通过 |
| 迈从 G87 V2 蓝牙键盘 | 项目作者于 2026-10-09 确认在 GameBoy 上实体测试正常 | 测试固件版本及配对码、组合键、断线重连的逐项结果未记录；不扩展为 FC 实测 |
| IINE-1001 | GameBoy 历史记录确认配对、方向输入及机身 Start／Select 补键流程 | 不适用于 IINE L167 等其他型号，也不代表重连耐久性通过 |
| FC 完整 BLE 验收 | 输入实现沿用 GameBoy | FC 阶段记录仍将完整配对、多键矩阵和断连重连列为待验收 |

Xbox 与 IINE 的细节见 [GameBoy 验证记录][gb-validation]。该固定版本文档中“尚无实体键盘测试”的旧状态，已由上述 G87 V2 的作者反馈更新；尚未提供的逐项结果仍保持未确认。

## 2 先确认设备真的使用 BLE

ESP32-C3 支持 Bluetooth LE，不支持经典蓝牙 BR/EDR。[Espressif 文档][idf-ble]说明了这个硬件边界。修改扫描过滤器、增加设备名称或换一个 HID 解析器，都不能给 C3 增加经典蓝牙。

不要仅凭“蓝牙 5.x”“支持 Xbox”“三模”或“能连接手机”判断兼容性。先记录机身型号、手柄固件版本和当前工作模式，再核对实际传输协议。同名产品的不同版本、同一手柄的不同模式可能不同。

[Bluepad32 的 Xbox 说明][bp-gamepads]区分了固件使用的协议：较早的 3.x／4.x Xbox 固件使用 BR/EDR，5.x 系列使用 BLE。本文的项目实测对象明确为 **1914／5.22.16.0**，不能据此承诺所有带 Xbox 标识的设备都能连接。

键盘也必须切换到蓝牙模式。本实现没有 USB HID 或 2.4 GHz 接收器路径。BLE 模式可用之后，还要确认它提供本实现能够处理的 HID 服务和报告。

## 3 让一个 Host 管理蓝牙控制器

两个项目使用以下输入链路：

```text
ESP32-C3 BLE Controller
  → BTstack（GAP、SMP、GATT、HID 客户端）
  → Bluepad32（设备识别、HID 报告解析）
  → gamepad.c（当前设备、配对状态、输入快照）
  → gb_input.c（游戏按键和菜单边沿）
  → 应用
```

从 [sdkconfig.defaults][config] 提取的配置起点：

```ini
CONFIG_BT_ENABLED=y
CONFIG_BT_CONTROLLER_ONLY=y
CONFIG_BT_NIMBLE_ENABLED=n
CONFIG_BLUEPAD32_PLATFORM_CUSTOM=y
CONFIG_BLUEPAD32_MAX_DEVICES=1
CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE=n
CONFIG_BTSTACK_AUDIO=n
```

这组配置依赖项目引入的 Bluepad32／BTstack 组件，不是复制到任意 ESP-IDF 工程就能工作的完整移植。控制器由 IDF 提供，Host 由 BTstack 持有；不要继续启动基线中的 NimBLE 广播 demo 去操作同一个控制器。新增输入功能时，同时检查组件依赖、BTstack 配置和实际初始化入口。

`gamepad_start()` 创建独立蓝牙任务，依次调用 `btstack_init()`、`uni_platform_set_custom()`、`uni_init()`，最后进入 BTstack 事件循环。连接成功后只更新输入快照，游戏循环自行读取；不要把模拟器、音频或 UI 渲染塞进蓝牙回调。[实现位置][gamepad]

## 4 扫描不到时先查发现规则

### 主动扫描还需要合并广播和扫描响应

设备名称、Appearance 和服务 UUID 可能分别出现在广播与扫描响应中。逐包判断“名称、类型必须同时存在”，会把真实设备漏掉；仅启用主动扫描仍不够。

项目使用 `gap_set_scan_parameters(1, 48, 48)` 开启主动扫描，再用缓存合并两类包。这里的两个 `48` 是 BTstack 的扫描参数单位，不能当作毫秒，也不是推荐给所有产品的功耗配置。

[广播合并实现][advertisement]保留以下规则：

- 底层缓存以 **地址和地址类型**共同作为键，最多 16 项，五秒无更新后失效；停止扫描时清空。
- 分包到达顺序不影响合并结果，后来的完整名称可以更新候选；短名称不能覆盖已有完整名称。
- 先验证 AD 字段长度，再更新缓存，避免截断包污染先前正确的信息。
- 名称按 UTF-8 字符边界截断，避免配对页出现半个汉字。

底层合并缓存与应用候选列表是两层状态。应用列表最多八项，当前按地址匹配；它并未完整传播地址类型，更不能被当作已经解决随机地址轮换、身份地址解析的通用设备注册表。新项目需要这些能力时，应端到端保留设备身份信息并补测。

### 设备名不应成为兼容性白名单

早期应用要求候选名称包含 Xbox，导致其他名称的手柄被排除。后续去掉名称要求，保留“用户选择目标设备”的连接门槛。名称用于展示，协议与报告用于决定能否使用。

只接纳 Gamepad 也会漏掉声明为 Joystick 的控制器。当前映射如下；右列的 COD 是 Bluepad32 内部分类，不是要求 BLE 广播携带经典蓝牙 COD。

| BLE Appearance | 含义 | 内部 COD |
| --- | --- | --- |
| `0x03C1` | Keyboard | `0x0540` |
| `0x03C2` | Mouse | `0x0580`，本应用拒绝 |
| `0x03C3` | Joystick | `0x0504` |
| `0x03C4` | Gamepad | `0x0508` |
| 缺失、零或 `0x03C0`，且声明 HID 服务 | 待确认的 HID 候选 | `0x0500` |

### HID 服务 UUID 只允许候选进入下一步

部分设备缺少具体 Appearance，但广播 HID 服务 `0x1812`。当前代码识别完整／不完整的 16 位 UUID 列表和蓝牙基础 128 位 UUID 列表；明确为鼠标或其他不同类型的设备，不会被这个兜底条件改判成手柄。

仅靠 UUID 进入的候选，在用户选择并连接后，还必须检查 Report Map：[分类实现][hid-classifier]只接受有数据 Input 的顶层 Gamepad、Joystick 或 Keyboard Application 集合，拒绝不支持、畸形或分类冲突的描述符。然后选择第一个符合要求的 HID 服务，在初始化解析器前复制其描述符；分类完成前及其他服务发来的报告会被忽略。[服务处理实现][ble]

这条额外分类路径针对“仅由 UUID 发现、类型待确认”的设备，不能描述成所有已有设备都改走了新解析流程。既没有可识别类型、也不广播 HID UUID 的设备，仍可能无法进入列表。

## 5 Xbox 已连接却没有输入时查配对阶段

### 加密成功不等于绑定和输入完成

Xbox 1914 的开发中曾出现：无绑定模式下链路完成加密，但手柄灯持续闪烁，也没有按键报告。该测试组合最终使用 Legacy Just Works 加绑定：

```c
sm_set_io_capabilities(IO_CAPABILITY_NO_INPUT_NO_OUTPUT);
sm_set_authentication_requirements(SM_AUTHREQ_BONDING);
```

随后历史真机记录确认手柄灯常亮、HID 服务连接、输入报告到达，以及重启后的密钥重加密。这个配置是上述型号与固件的经验，不应作为所有设备的统一安全策略。Just Works 不提供 MITM 身份认证；有相应安全要求的产品需要另行选择和验证策略。键盘的 IO 能力也不能照抄此处。

移植时读实际执行的语句，不要只读文件里的历史试验注释。例如 [`uni_bt_le_setup()`][ble] 中仍保留不同配对组合的旧说明，生效配置应以 `sm_set_authentication_requirements()` 的调用为准。

### 密钥分发集合不一致会卡住配对

项目对 [BTstack SMP 实现][sm] 做了两处关联修补：

1. 未请求 bonding 时，不请求用于长期绑定的密钥。
2. 作为发起方处理配对响应时，将对端声明的密钥分发位与本机原始请求取交集。

第二处的关键逻辑为：

```c
keys_to_send &= sm_pairing_packet_get_initiator_key_distribution(setup->sm_m_preq);
keys_to_receive &= sm_pairing_packet_get_responder_key_distribution(setup->sm_m_preq);
```

如果状态机等待本机未请求、对端也不会发送的密钥，单纯加长超时不会使配对完整结束。排查时分开看 Pairing Complete、Re-encryption Complete、Device Information 查询、HID Service Connected 和实际 Input Report，明确停在哪一步。

这些修改针对固定版本的本地依赖。升级 BTstack 时先比对上游实现，不能不加检查地重复补丁。本文定向主机测试未覆盖完整 SMP 无线交互，配对仍需真实设备验证。

## 6 键盘要有完整的配对码交互

键盘与免输入手柄的差别不只是映射表。对于已识别键盘和类型待确认的 HID 候选，应用选择 `IO_CAPABILITY_DISPLAY_ONLY`，这样外设需要配对码时，设备能够显示数字供用户在键盘上输入。

[当前实现][gamepad]处理 `SM_EVENT_PASSKEY_DISPLAY_NUMBER`、`SM_EVENT_PASSKEY_DISPLAY_CANCEL`、`SM_EVENT_PAIRING_COMPLETE` 和 `SM_EVENT_REENCRYPTION_COMPLETE`。界面显示六位数字，保留前导零；用户在键盘输入后按 Enter。配对时的 Enter 与游戏的 Start 映射无关。

需要一起处理的边界：

- 收到配对码事件后，将该设备原有 20 秒连接定时器延长至 60 秒，并维护 60 秒的配对码显示定时器，给用户留出输入时间。
- 只显示当前选中设备的配对码，其他设备的安全事件不能覆盖当前界面。
- 成功、失败、取消、断开和超时都要清理提示；界面消失不代表配对成功。
- 用户取消后，在蓝牙线程取消尚未完成的连接并清理非当前设备；迟到的 ready 回调还要重新检查选择状态。

六位码、60 秒和单设备连接是本项目的交互设计。G87 V2 已有作者实测正常的反馈，但不能据此把错误配对码、取消竞态或连续重连等未逐项记录的测试都标为通过。

## 7 按 HID 描述符解析键盘报告

### 不要把每个键盘报告都当成固定八字节

键盘可能使用数组式按键、带 Report ID 的报告，或每个键占一位的位图报告。直接用固定偏移取第几个字节，容易把 Report ID 当按键或漏掉组合键。本项目复用 Bluepad32／BTstack 的描述符解析器，再将 Keyboard/Keypad Usage 映射为应用动作。

项目中的示例映射为：

| 按键 | HID Usage | 游戏动作 |
| --- | --- | --- |
| W／A／S／D | `0x1A`／`0x04`／`0x16`／`0x07` | 上／左／下／右 |
| J／K | `0x0D`／`0x0E` | A／B |
| L／I | `0x0F`／`0x0C` | Start／Select |

这是物理按键 Usage 映射，不依赖大小写、输入法或字符输入。不同应用可以改变动作映射，不需要改写底层 HID 解析器。当前手柄应用路径读取 D-pad、A／B、Select／Start；并没有把模拟摇杆轴自动映射成方向键。

### 独立媒体键报告不能清空长按状态

一个容易被单键测试遗漏的问题：按住 D 和 J 时，键盘另外发送音量键报告。如果解析器在每个报告开头都清空键盘状态，应用会错误地认为方向和动作键已经松开。

[本地修复][keyboard-parser]在普通键盘的新报告开始时只重置解析游标；只有遇到 Keyboard/Keypad 页时才重建键盘状态。独立 Consumer／媒体键报告不覆盖仍按住的字母键，后续真实键盘释放报告仍会清空它们。原有 JX-05 特殊解析分支保留。

[真实解析器主机测试][keyboard-test]覆盖了“D＋J＋K 按住 → 媒体键按下 → 媒体键松开 → 字母键全部松开”，并覆盖带／不带 Report ID 的数组和位图报告。这个修复解决的是独立媒体报告干扰，不等于任意多 Report ID 键盘的状态合并或无限 NKRO 都已验证。

### 必须定义异常和释放行为

[`gb_input.c`][input]将 `ErrorRollOver`、`POSTFail`、`ErrorUndefined`（Usage 1～3）视为无效输入，释放游戏按键，避免方向一直卡住；相反方向同时按下时互相抵消。普通释放报告更新当前按键集合，断连时整个输入快照归零。

游戏需要持续的按住状态，菜单需要按下边沿。菜单首次进入或重新连接时，以当前快照作为基线，避免已按住的确认键立即触发菜单。不要让键盘自动重复报告变成多次菜单确认。

## 8 线程和生命周期与协议同样重要

Bluepad32 的 `*_unsafe()` 调用留在 BTstack 线程中。UI 或游戏任务只提交请求，使用 `btstack_run_loop_execute_on_main_thread()` 应用扫描策略；执行时再次读取当前连接状态，避免“连接刚 ready，排队中的旧请求又打开扫描”。

当前实现用短临界区保护输入和发现快照，并只接收 `s_owner` 对应设备的报告。锁内不做渲染或阻塞 I/O。设备断开时清空 owner、释放所有按键，再按策略恢复扫描；重新进入或取消配对页时清理旧候选，避免八项列表被旧设备占满。

进入 Wi-Fi 管理页时，项目关闭 BLE 扫描，退出后恢复；关闭扫描不等于关闭控制器或断开既有连接，也不等于完成 Wi-Fi／BLE 高负载共存验证。对无 PSRAM 的 C3，应观察内部剩余堆、最大连续块、蓝牙任务栈和回调耗时，再决定资源配置。

游戏画面卡住、音乐继续或回到主页，不能直接判定为蓝牙断线。GameBoy 曾有这类问题的记录，但部分观察仍能收到输入报告。用断连事件和报告时间定位链路，再分别检查模拟器、存档、音频和任务调度，不要把一次 BLE 修复写成所有卡顿问题的解决证明。

## 9 按最后成功阶段定位问题

建议按以下顺序记录状态；这是排查流程，不是项目中一个现成的统一枚举：

```text
控制器就绪 → 扫描收到广播 → 候选列表 → 用户选择
→ 链路建立 → 配对或重加密成功 → HID 服务就绪
→ 报告解析 → 应用动作 → 断开释放与重连
```

| 现象 | 优先检查 | 修复或验证方向 |
| --- | --- | --- |
| 列表完全没有设备 | 协议、工作模式、配对模式、原始广播 | 先确认 BLE；不能靠增加名字支持 BR/EDR |
| 抓到了广播但列表没有设备 | 分包字段、Appearance、HID UUID、名称过滤 | 合并广播与扫描响应，区分 Gamepad 和 Joystick |
| 列表一直被旧设备占满 | 应用八项候选状态 | 新扫描／取消时重置；不要误清正在连接的选择 |
| 链路已加密但 Xbox 灯闪、无输入 | bonding 策略、SMP 分发状态 | 对照固定型号经验，确认配对结束与 HID 报告 |
| 键盘连接等待后超时 | IO 能力、配对码事件、连接定时器 | 显示六位码、延长输入时间、保留取消路径 |
| 按媒体键后游戏方向松开 | 是否每份报告都重置键盘状态 | 区分 Keyboard/Keypad 与 Consumer 报告 |
| 断连后角色一直移动 | owner 清理、全量快照释放 | 用“按住方向时关闭设备”验证 |
| 菜单一进入就确认或连续跳项 | 边沿基线、重复报告 | 首次进入／重连初始化菜单状态 |
| HID UUID 存在但连接失败 | Report Map、所选 HID 服务、Device Information 查询 | UUID 只是候选证据；当前代码仍在设备信息查询失败时断开 |
| 启动出现 `0x0c05`／状态 `0x01` | 是否向 C3 发送 BR/EDR 初始化命令 | 按目标能力跳过 SSP 与 inquiry filter，同时正常完成 BLE 初始化 |

最后一项是实际出现过的独立问题：`HCI_Set_Event_Filter` 用于经典蓝牙查询过滤，BLE-only 控制器返回 Unknown HCI Command。项目在 [`uni_bt_setup.c`][setup] 按编译能力和运行配置跳过这组命令，不是把错误日志屏蔽掉。旧日志在这条错误之后仍有 BLE 扫描，因此它也不能单独证明扫描已失败。[历史记录][gb-validation]

日志至少区分配对、重加密、服务查询、ready、输入、断连和重连。记录型号、固件版本、输入模式、失败阶段和状态码；分享日志前移除设备地址、密钥及其他设备标识，不要把输入设备连接密钥打印到公开问题中。

## 10 验证需要覆盖状态转换

2026-10-09 针对 GameBoy 固定提交重新编译运行以下七个主机测试程序，**7 通过、0 失败**。使用宿主 GCC、C11、`-O2 -Wall -Wextra -Werror`；测试调用项目实际逻辑，蓝牙无线和平台依赖使用已有测试桩。它们不执行真实配对，也不覆盖完整 SMP 链路。

| 测试程序 | 主要覆盖 |
| --- | --- |
| `test_gamepad_discovery` | 候选、选择、取消、刷新、PIN 状态 |
| `test_bt_le_advertisement` | 分包顺序、地址类型隔离、过期、畸形 AD、HID UUID |
| `test_bt_le_hid` | 支持的顶层集合、冲突类型、截断与边界 |
| `test_gb_input` | 映射、组合输入、异常 Usage、松开、菜单边沿 |
| `test_gb_keyboard_hid` | 真实 HID 解析器、Report ID、媒体键、数组／位图 |
| `test_bt_setup` 的 ESP32C3 构建 | BLE-only 初始化 |
| `test_bt_setup` 的 ESP32 构建 | 双模以及运行时禁用 BR/EDR 的初始化路径 |

测试源码和编译入口见 [tests][tests]、[validate.sh][validate]。这里的数量是七个程序运行，不是七次设备测试，也不是整个项目门禁通过。本文整理期间未重新构建或烧录固件。

实际接入新设备时，逐项记录以下验收：

- 首次配对：真实设备进入正确模式，列表可选，连接后收到按下和松开报告。
- 键盘配对码：有／无码连接、前导零、错误码、超时、取消、取消后迟到事件。
- 输入：方向与动作键组合、长按、部分松开、全部松开、媒体键插入、相反方向。
- 断连：按住方向时关机或离开范围，应用立即释放；重连后不误触菜单。
- 绑定：设备和主机分别重启、已有密钥复用、重新配对；记录重连次数及观察时间。
- 应用负载：游戏、声音和显示同时运行，以及进入／退出 Wi-Fi 管理页时的行为。

每条结果附应用提交或固件身份、外设型号／固件／模式和测试条件。“用户实际使用正常”可以记录为有效反馈，但不能替代没有逐项执行的异常路径测试。FC 的模拟机测试入口不运行正式 BLE 路径，模拟机通过不能作为无线验收。[FC 验证边界][fc-validation]

## 11 复用时按层提取

| 要复用的能力 | 固定版本源码入口 |
| --- | --- |
| 发现列表、选择、PIN、取消、输入快照和线程切换 | [`main/gamepad.c`][gamepad]、[`main/gamepad_discovery.c`][discovery] |
| 输入映射与菜单边沿 | [`main/gb_input.c`][input] |
| 广播合并与 HID UUID 兜底 | [`uni_bt_le_advertisement.c`][advertisement]及对应头文件 |
| 描述符分类与服务选择 | [`uni_bt_le_hid.c`][hid-classifier]、[`uni_bt_le.c`][ble] |
| BLE-only 初始化 | [`uni_bt_setup.c`][setup] |
| 键盘媒体报告状态保留 | [`uni_hid_parser_keyboard.c`][keyboard-parser] |
| SMP 密钥分发修补 | [`components/btstack/src/ble/sm.c`][sm] |

移植应先建立一个能显示“设备状态与按键快照”的最小入口，验证发现、配对和松开，再接入业务。同步带上相应头文件、构建依赖和测试；保留第三方来源、许可证和本地修改说明。不要把 GameBoy／FC 的模拟器、ROM、分区或完整镜像作为连接 BLE 输入设备的前置条件。

[gb]: https://github.com/PhoenixZHC/folotoy-gameboy/tree/f221fe83fc40ec07f37f93bb19ce3d74879e8718
[fc]: https://github.com/PhoenixZHC/folotoy-fc/tree/d1206fae4feb650a1c7d2a7a211cda4205ce3381
[gb-validation]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/docs/development/gameboy-validation.zh_CN.md
[fc-validation]: https://github.com/PhoenixZHC/folotoy-fc/blob/d1206fae4feb650a1c7d2a7a211cda4205ce3381/docs/validation.md
[idf-ble]: https://docs.espressif.com/projects/esp-idf/en/v5.5.3/esp32c3/api-guides/ble/overview.html
[bp-gamepads]: https://bluepad32.readthedocs.io/en/latest/supported_gamepads/#xbox-wireless-model-1914-3-buttons
[config]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/sdkconfig.defaults
[gamepad]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/main/gamepad.c
[discovery]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/main/gamepad_discovery.c
[input]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/main/gb_input.c
[advertisement]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/bt/uni_bt_le_advertisement.c
[hid-classifier]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/bt/uni_bt_le_hid.c
[ble]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/bt/uni_bt_le.c
[setup]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/bt/uni_bt_setup.c
[keyboard-parser]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/bluepad32/parser/uni_hid_parser_keyboard.c
[keyboard-test]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/tests/test_gb_keyboard_hid.c
[sm]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/components/btstack/src/ble/sm.c
[tests]: https://github.com/PhoenixZHC/folotoy-gameboy/tree/f221fe83fc40ec07f37f93bb19ce3d74879e8718/tests
[validate]: https://github.com/PhoenixZHC/folotoy-gameboy/blob/f221fe83fc40ec07f37f93bb19ce3d74879e8718/tools/validate.sh
