<p align="right">
  <strong>简体中文</strong> · <a href="softap-provisioning.md">English</a>
</p>

# AI Passport 上的 SoftAP 配网、DHCP 与认证弹窗

记录于 AI Passport 固件的 SoftAP 网页配网工作之后（媒体渲染器 App 上的 Captive Portal、DHCP 与 APSTA 切网工作）。本条目记录如何搭 SoftAP + DHCP + HTTP 配网流程、让手机自动弹出认证页，以及如何避免表单缓冲溢出。它不限定具体应用。无论 AI Passport 最终运行音乐、语音、传感器、MQTT 或其他应用，Wi-Fi 密码、服务器地址、设备名称、绑定码等字段都可以使用同一套配网流程。

## AI Passport 硬件边界

| 项目 | AI Passport 实际配置 | 对配网的影响 |
| --- | --- | --- |
| MCU | ESP32-C3，8 MB Flash，无 PSRAM | SoftAP、STA、HTTP、DNS、LVGL 和任务栈都占内部 RAM |
| 无线 | 仅 2.4 GHz Wi-Fi 802.11 b/g/n | 配网页必须明确提示用户选择 2.4 GHz 网络 |
| 屏幕 | 240×320 ST7789P3 | 可显示热点名、`192.168.4.1`、连接状态和失败原因，作为手机弹窗失败时的兜底 |
| 功能按键 | UP、DOWN、OK 共用 GPIO0 ADC 电阻梯 | 可用长按 OK 清除错误配置，但回调中不能直接执行联网或 NVS 重操作 |
| NVS/网络 | Wi-Fi 使用 ESP-IDF `esp_netif` 和默认事件循环 | 网络基础设施只初始化一次，重进配网页时只重建自身服务和任务 |
| 日志接口 | USB Serial/JTAG，GPIO18/19 | 配网问题优先看原生 USB 日志，不要占用 GPIO21 的默认 UART0 TX 映射 |

AI Passport 没有 PSRAM，因此不建议在配网页嵌入大图片、Web 字体或复杂脚本。HTML 越简单，HTTP 任务、DNS 服务和屏幕 UI 同时运行时越稳定。硬件定义以 [`bsp_pins.h`](../../../components/bsp/include/bsp_pins.h) 和 [`sdkconfig.defaults`](../../../sdkconfig.defaults) 为准。

## 1. 先分清四件事

配网经常被统称为"开热点弹网页"，实际上是四个独立环节：

1. **SoftAP**：ESP32 创建一个 Wi-Fi 热点，手机能连上。
2. **DHCP**：手机从 ESP32 获得 IP、网关和 DNS 信息。
3. **HTTP 配网页**：ESP32 在 `192.168.4.1:80` 提供表单。
4. **Captive Portal**：系统发现这个网络需要认证，自动弹出小浏览器。

只完成前三项，用户仍可手动打开 `http://192.168.4.1`，但手机不一定自动弹窗。自动弹窗通常需要：

- DNS 把任意域名解析到 SoftAP IP；
- HTTP 对探测路径返回页面或重定向；
- 可选的 DHCP Option 114 直接告诉客户端认证页地址。

最小可用版本只需要 SoftAP、默认 DHCP 和 HTTP 表单，用户手动打开 `http://192.168.4.1`。完整版本再增加 DNS 重定向、系统探测路径和 DHCP Option 114，提高自动弹窗概率。无论是否自动弹窗，都应保留手动地址作为兜底。

## 2. 推荐的整体流程

```text
启动配网任务
  ↓
初始化 NVS、esp_netif、默认事件循环
  ↓
创建 AP netif 和 STA netif
  ↓
Wi-Fi 使用 APSTA 模式
  ↓
启动 SoftAP（默认 192.168.4.1）
  ├─ DHCP 给手机分配地址
  ├─ DNS 把域名指向 192.168.4.1
  └─ HTTP 提供配网页和探测重定向
  ↓
用户提交 Wi-Fi 和业务配置
  ↓
HTTP 回调只校验、复制并通知后台任务
  ↓
后台任务连接目标 Wi-Fi，等待 STA_GOT_IP
  ↓
验证目标服务或业务接口
  ↓
验证成功后写入完整配置并进入主应用
```

网络连接、业务验证和测试都不要放在 HTTP 回调或按键回调里，否则会堵住 HTTP 服务和 LVGL。

## 3. SoftAP 和 DHCP 怎么写

### 3.1 使用默认 AP netif 就自带 DHCP Server

ESP-IDF 5.5 的默认 AP 配置带 `ESP_NETIF_DHCP_SERVER` 标志。下面这句会创建默认 SoftAP 网络接口，并使用默认地址 `192.168.4.1`：

```c
esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
```

之后再初始化并启动 Wi-Fi：

```c
wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
ESP_ERROR_CHECK(esp_wifi_init(&cfg));

wifi_config_t ap = {0};
strlcpy((char *)ap.ap.ssid, "AI-Passport-1234", sizeof(ap.ap.ssid));
ap.ap.ssid_len = strlen((char *)ap.ap.ssid);
ap.ap.channel = 1;
ap.ap.max_connection = 2;
ap.ap.authmode = WIFI_AUTH_OPEN;

ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap));
ESP_ERROR_CHECK(esp_wifi_start());
```

如果不需要自定义地址池，不必自己再启动一套 DHCP Server。AI Passport 重进配网页时若重复创建默认 AP netif 或重复启动 DHCP，常见结果是句柄泄漏、状态冲突或再次进入时崩溃。

### 3.2 需要改 AP 地址时，先停 DHCP 再改

```c
esp_netif_ip_info_t ip = {0};
IP4_ADDR(&ip.ip,      192, 168, 4, 1);
IP4_ADDR(&ip.gw,      192, 168, 4, 1);
IP4_ADDR(&ip.netmask, 255, 255, 255, 0);

ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip));
ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
```

IP、网关、DNS 重定向目标、HTTP 重定向地址和 DHCP Option 114 必须使用同一个 AP 地址。改了一处漏改其他地方，会出现"手机拿到地址，但页面打不开"或"弹窗打开后循环跳转"。

### 3.3 如何确认 DHCP 正常

串口至少记录：

- SoftAP 启动成功和实际 IP。
- `WIFI_EVENT_AP_STACONNECTED`：手机已连接热点。
- DHCP 分配日志：手机获得了类似 `192.168.4.2` 的地址。
- HTTP 根路径是否收到请求。

如果手机显示"正在获取 IP"后断开，先检查 DHCP 和 AP netif，不要先怀疑 HTML 页面。页面还没有机会参与这个阶段。

## 4. STA 连接和 DHCP Client 怎么写

默认 STA netif 自带 DHCP Client。设置家庭 Wi-Fi 后调用 `esp_wifi_connect()`，以 `IP_EVENT_STA_GOT_IP` 作为真正联网成功的标准：

```c
static volatile bool sta_got_ip;

static void ip_event_handler(void *arg,
                             esp_event_base_t base,
                             int32_t id,
                             void *data)
{
    if (id == IP_EVENT_STA_GOT_IP) {
        sta_got_ip = true;
    }
}
```

不要把 `WIFI_EVENT_STA_CONNECTED` 当成完成。它只说明已关联到路由器，还不能证明已经获得 IP、网关和 DNS。

等待要有超时，例如 20 秒；超时后显示明确原因，并允许用户重新提交。不要无限循环重连，让配网页和主任务都失去响应。

### 一个很容易踩的网络地址错误

在设备热点测试时，提供本地服务的电脑或手机可能是 `192.168.4.2`，设备是 `192.168.4.1`。但设备切换到家庭 Wi-Fi 后，`192.168.4.2` 通常已经不是原来的服务端。

如果设备后续要访问局域网服务，配置地址必须使用服务端在家庭局域网中的地址，例如：

```text
http://192.168.5.18:18080
```

固件应在正式配置里拒绝指向自身临时 SoftAP 网段的服务地址，并提示用户填写切网后仍然可达的局域网地址或域名。这个校验能避免配网页阶段可用、切网后永远连接失败的问题。

## 5. 自动弹出认证页怎么做

Espressif 官方 captive portal 示例使用两种可以同时启用的方法：DNS/HTTP 引流，以及 DHCP Option 114。参考：[ESP-IDF Captive Portal 示例](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/captive_portal) 和 [ESP-IDF HTTP Server 文档](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c3/api-reference/protocols/esp_http_server.html)。

### 5.1 DNS：所有域名都回答 SoftAP IP

启动一个 UDP 53 端口的 DNS 服务，对所有 A 查询都返回 AP 地址：

```c
dns_server_config_t dns =
    DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
dns_server_handle_t dns_handle = start_dns_server(&dns);
if (dns_handle == NULL) {
    ESP_LOGE(TAG, "DNS server start failed");
    return ESP_FAIL;
}
```

`dns_server` 不是一个可以随意假设存在的系统 API。移植时应直接参考并带入 ESP-IDF 官方 captive portal 示例里的 DNS server 组件和 CMake 配置。

DNS 服务必须在退出配网时停止，否则再次进入配网页时可能出现端口 53 被占用、任务泄漏或旧 netif 句柄仍被访问。

### 5.2 HTTP：接住系统探测路径和未知路径

常见探测包括：

- Android：`/generate_204`
- Apple：`/hotspot-detect.html`
- Windows：`/connecttest.txt`、`/ncsi.txt`

不要只注册 `/` 和 `/save`。可为探测路径注册处理器，并为其他 HTTP 404 返回重定向：

```c
static esp_err_t redirect_to_portal(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    // iOS 等客户端需要响应正文，只有空重定向时可能不弹认证页。
    return httpd_resp_send(req,
                           "Redirect to the captive portal",
                           HTTPD_RESP_USE_STRLEN);
}

static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t err)
{
    return redirect_to_portal(req);
}
```

认证探测行为会随系统版本变化，不要只针对一个固定 Host。最稳妥的策略是：配网期间，除静态资源、根页面和保存接口外，其他普通 HTTP 请求统一引到根页面。

HTTPS 不能这样透明重定向，因为证书域名不匹配。不要在 ESP32 上拦截 443 伪装目标网站；Captive Portal 只负责引导普通 HTTP 探测和 DHCP 提示。

### 5.3 DHCP Option 114：直接声明认证页

ESP-IDF 5.5 可在 DHCP Offer 中提供认证页 URL。必须在修改选项前停止 DHCP Server，设置后再启动：

```c
static char captive_uri[] = "http://192.168.4.1/";

ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
ESP_ERROR_CHECK(esp_netif_dhcps_option(
    ap_netif,
    ESP_NETIF_OP_SET,
    ESP_NETIF_CAPTIVEPORTAL_URI,
    captive_uri,
    strlen(captive_uri)));
ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
```

对应能力需要按 ESP-IDF 官方示例启用 `CONFIG_ESP_ENABLE_DHCP_CAPTIVEPORTAL`。URI 的存储在 DHCP Server 生命周期内必须持续有效，因此使用静态缓冲或长期有效的全局内存，不要传入函数栈上的临时数组。

Option 114、DNS 引流和 HTTP 探测可以一起启用。不同手机支持不同，三者配合比只押一个机制可靠。

## 6. 配网表单怎么写，才能避免缓冲溢出

### 6.1 请求总长度先设硬上限

小型配网页可先把表单上限设为 1024 字节，再根据实际字段测量调整：

```c
#define PROV_FORM_MAX 1024

if (req->content_len <= 0 || req->content_len >= PROV_FORM_MAX) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "form too large");
    return ESP_FAIL;
}
```

边界要为结尾的 `\0` 留空间，所以使用 `>= PROV_FORM_MAX` 拒绝。

### 6.2 `httpd_req_recv()` 不保证一次收完

必须循环读取：

```c
char body[PROV_FORM_MAX] = {0};
int received = 0;

while (received < req->content_len) {
    int n = httpd_req_recv(req,
                           body + received,
                           req->content_len - received);
    if (n == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;  // 只允许有限次数重试，并记录超时
    }
    if (n <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR,
                            "request read failed");
        return ESP_FAIL;
    }
    received += n;
}
body[received] = '\0';
```

不要用 `recv(req, body, sizeof(body))` 后直接解析，也不要相信浏览器一定一次发送完整表单。

### 6.3 三层长度都要限制

HTML 的 `maxlength` 只改善用户体验，不能作为安全边界。必须同时限制：

| 字段 | 协议限制 | C 缓冲建议 |
| --- | --- | --- |
| SSID | 最多 32 字节 | `char ssid[33]` |
| Wi-Fi 密码 | 最多 64 字节 | `char password[65]` |
| 服务地址 | 示例最多 127 字节 | `char service_url[128]` |
| 绑定码或令牌 | 示例最多 95 字节 | `char pairing_token[96]` |

还要注意 URL 编码：一个字节可能写成 `%XX`，HTTP body 的编码长度会比解码后长。解码函数必须知道目标容量，并在输出放不下时返回失败，不能静默截断后继续注册。

推荐接口：

```c
bool url_decode_checked(char *dst,
                        size_t dst_len,
                        const char *src,
                        size_t src_len);
```

解析完成后再次检查：必填字段是否存在、服务地址协议是否合法、地址是否误填成临时 SoftAP 网段、字符串是否完整终止。

### 6.4 不要把大结构放在小任务栈上

1024 字节 body、表单结构、HTTP 解析临时变量和日志格式化缓冲如果都放在处理任务栈上，很容易形成峰值。可以：

- 保持网页短小，不内嵌大图片或字体。
- 大 body 用受限的堆分配，并检查失败；处理完立即释放。
- 减少同时打开的 socket；配网一般只需一两个连接。
- HTTP 回调完成校验和复制后马上返回，耗时连接交给 worker task。

"加大 HTTP Server 栈"只能解决栈不足，不能解决堆、socket、DMA 或 LVGL 内存不足。先确认溢出发生在哪一类内存。

## 7. 配置保存和注册的正确时机

推荐区分"待验证配置"和"正式配置"：

1. 用户提交后先保存到临时结构。
2. STA 连接成功并获得 IP。
3. 访问健康检查、设备绑定或其他业务验证接口。
4. 验证成功后设置 `configured = true`，一次性提交 NVS。
5. 下次开机只在所有必填字段和 `configured` 状态都有效时进入主应用。

如果为了断电恢复而提前写入 NVS，也必须让启动逻辑拒绝 `configured = false` 的半成品配置。不要因为 NVS 里有 SSID 就误判配网完成。

如果业务需要设备身份，应由每台设备首次启动随机生成，公开固件中不要内置所有设备共用的密钥。一次性绑定码只用于首次绑定，不应长期当作设备鉴权密钥。

## 8. AI Passport 使用 APSTA 模式的常见坑

- AP 和 STA 共用一套 Wi-Fi 射频。STA 连接路由器后，SoftAP 信道可能随 STA 信道变化，手机短暂掉线不一定是 DHCP 崩坏。
- 手机常因"热点无互联网"自动切回蜂窝网络或其他 Wi-Fi。配网页必须明确提示保持连接。
- 电脑防火墙可能拦截本地服务端口。设备拿到 STA IP 不代表能访问目标服务。
- 只监听 `127.0.0.1` 的电脑服务不能被设备访问，必须监听局域网接口。
- AI Passport 只支持 2.4 GHz Wi-Fi；同名双频网络、企业认证网络和隐藏 SSID 需要单独验证。
- AI Passport 的 BLE、Wi-Fi、LVGL 和音频都消耗内部 RAM；配网阶段如果不需要 BLE 和音频，应停止或延后初始化。
- Wi-Fi 断开事件里只更新状态和安排重试，不要直接销毁 netif 或在事件回调里做阻塞操作。
- 退出配网页时按相反顺序停止 DNS、HTTP、Wi-Fi，注销事件处理器，再销毁 AP/STA netif；每个句柄只释放一次。

## 9. 常见故障定位表

| 现象 | 更可能的原因 | 排查重点 |
| --- | --- | --- |
| 手机连不上热点 | AP 未启动、认证参数错误、内存不足 | Wi-Fi 启动返回值、AP 事件、剩余堆 |
| 一直"获取 IP" | DHCP Server/netif 状态异常 | 是否用了默认 AP netif、是否重复 stop/start |
| 已拿到 `192.168.4.x` 但网页打不开 | HTTP 未启动、端口错误、地址不一致 | 访问 `http://192.168.4.1`、HTTP 日志 |
| 手动能打开但不自动弹窗 | 没有 DNS 引流、探测路由或 Option 114 | UDP 53、404 重定向、DHCP Offer |
| 有些手机弹、有些不弹 | 系统探测策略不同或缓存 | DNS、Option 114、各系统探测 URL |
| 提交后立刻 400 | body 超限、字段缺失、URL 解码失败 | `content_len`、字段编码长度、日志 |
| 提交时重启 | 请求任务栈或堆峰值过高 | body 放置位置、HTTP 栈、并发 socket |
| 配置成功后主应用连不上服务 | 服务地址写成临时热点下的 `192.168.4.2` | 改成切网后可达的局域网 IP 或域名 |
| STA 已连接但请求失败 | 尚未获得 IP/DNS、防火墙拦截 | 等 `IP_EVENT_STA_GOT_IP`、端口可达性 |
| 第二次进入配网页崩溃 | DNS/HTTP/task/netif 没有完整释放 | 退出顺序、句柄置空、重复注册事件 |

## 10. 建议的真机测试矩阵

至少覆盖：

1. Android、iPhone、Windows 各连接一次，记录是否自动弹窗。
2. 手动访问 `192.168.4.1`，确保自动弹窗失败时仍有兜底入口。
3. 正确密码、错误密码、空密码、32 字节 SSID、64 字节密码。
4. 超长表单、缺字段、非法 `%` 编码和多次快速提交。
5. 服务不可达、绑定码错误、防火墙拦截和服务重启。
6. 配网中断电，确认不会把半成品配置当成成功配置。
7. 连续进入和退出配网页 20 次，检查任务数、socket、netif 和剩余堆没有持续下降。
8. APSTA 切换信道后，配网页和注册流程是否仍能完成。

## 11. 可以直接复用的结论

- AI Passport 是无 PSRAM 的 ESP32-C3 设备，配网页必须保持轻量，并把 SoftAP、DNS、HTTP、LVGL 和任务栈一起核算。
- 利用 240×320 屏幕持续显示热点名和 `192.168.4.1`，不要把成功入口完全押在手机自动弹窗上。
- UP、DOWN、OK 是 GPIO0 ADC 电阻梯按键；清除配置等动作可由长按触发，但耗时工作必须交给后台任务。
- 默认 SoftAP netif 已带 DHCP Server；没有特殊地址需求时不要重复造一套。
- 连接路由器要等 `IP_EVENT_STA_GOT_IP`，不能只等 Wi-Fi connected。
- DHCP 负责分地址，不负责自动弹窗；自动弹窗需要 DNS/HTTP 引流，最好再加 Option 114。
- 自动弹窗不是百分之百可靠，始终在设备屏幕上保留 `http://192.168.4.1` 手动入口。
- 表单要同时限制 HTTP body、编码字段和解码后目标缓冲，溢出必须明确报错。
- HTTP 回调只做短操作，联网和业务验证放后台任务。
- 局域网服务地址必须在设备切换到目标 Wi-Fi 后仍然可达，不能沿用临时热点下的客户端地址。
- 所有网络服务都要有对称的启动和停止流程，才能安全重进配网。

## 分流

这是通用、上游也受益的硬件与设计经验，因此作为文档 PR 提交回上游 `FoloToy/ai-passport`。
