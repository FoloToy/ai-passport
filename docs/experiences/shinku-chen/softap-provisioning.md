<p align="right">
  <a href="softap-provisioning.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# SoftAP Provisioning, DHCP, and Captive Portal on AI Passport

Captured after the SoftAP web-provisioning work on the AI Passport firmware (the captive-portal, DHCP, and APSTA switch work on the media-renderer app). This entry records how to build the SoftAP + DHCP + HTTP provisioning flow, get a phone to auto-open the captive portal, and avoid form-buffer overflow. It is not tied to one application. Whether the AI Passport ends up running music, voice, sensors, MQTT, or another app, the Wi-Fi password, server address, device name, and binding code can all use the same provisioning flow.

## AI Passport hardware boundary

| Item | AI Passport actual configuration | Impact on provisioning |
| --- | --- | --- |
| MCU | ESP32-C3, 8 MB flash, no PSRAM | SoftAP, STA, HTTP, DNS, LVGL and task stacks all occupy internal RAM |
| Wireless | 2.4 GHz Wi-Fi 802.11 b/g/n only | The provisioning page must clearly tell the user to choose a 2.4 GHz network |
| Display | 240×320 ST7789P3 | Can show the hotspot name, `192.168.4.1`, connection state, and failure reason, as a fallback when the phone popup fails |
| Function keys | UP, DOWN, OK share a GPIO0 ADC resistor ladder | Long-press OK can clear bad config, but the callback must not do networking or heavy NVS work |
| NVS/network | Wi-Fi uses ESP-IDF `esp_netif` and the default event loop | Network infrastructure is initialized once; re-entering the provisioning page only rebuilds its own services and tasks |
| Log interface | USB Serial/JTAG, GPIO18/19 | Look at the native USB log first for provisioning problems; do not take GPIO21's default UART0 TX mapping |

AI Passport has no PSRAM, so do not embed large images, web fonts, or complex scripts in the provisioning page. The simpler the HTML, the more stable it runs while the HTTP task, DNS service, and screen UI are all active at once. Hardware definitions follow [`bsp_pins.h`](../../../components/bsp/include/bsp_pins.h) and [`sdkconfig.defaults`](../../../sdkconfig.defaults).

## 1. First separate four things

Provisioning is often lumped together as "open a hotspot and pop a web page", but it is actually four independent pieces:

1. **SoftAP**: the ESP32 creates a Wi-Fi hotspot that a phone can connect to.
2. **DHCP**: the phone gets its IP, gateway, and DNS from the ESP32.
3. **HTTP provisioning page**: the ESP32 serves a form at `192.168.4.1:80`.
4. **Captive Portal**: the system discovers this network needs auth and auto-opens a small browser.

Completing only the first three lets the user manually open `http://192.168.4.1`, but the phone may not auto-pop. Auto-pop usually needs:

- DNS resolving any domain to the SoftAP IP;
- HTTP returning a page or redirect for the probe path;
- optionally DHCP Option 114 telling the client the auth page address directly.

A minimal usable version only needs SoftAP, default DHCP, and the HTTP form; the user manually opens `http://192.168.4.1`. A full version adds DNS redirect, the system probe path, and DHCP Option 114 to raise the auto-pop probability. Whether or not the popup opens, keep the manual address as a fallback.

## 2. Recommended overall flow

```text
start provisioning task
  ↓
initialize NVS, esp_netif, default event loop
  ↓
create AP netif and STA netif
  ↓
Wi-Fi uses APSTA mode
  ↓
start SoftAP (default 192.168.4.1)
  ├─ DHCP assigns the phone an address
  ├─ DNS points domains to 192.168.4.1
  └─ HTTP serves the provisioning page and probe redirect
  ↓
user submits Wi-Fi and business config
  ↓
HTTP callback only validates, copies, and notifies a background task
  ↓
background task connects to the target Wi-Fi, waits for STA_GOT_IP
  ↓
validate the target service or business interface
  ↓
on success write the complete config and enter the main app
```

Network connection, business validation, and testing must not go in the HTTP callback or the button callback, otherwise they block the HTTP service and LVGL.

## 3. How to write SoftAP and DHCP

### 3.1 The default AP netif already brings a DHCP server

ESP-IDF 5.5's default AP config carries the `ESP_NETIF_DHCP_SERVER` flag. This line creates the default SoftAP network interface and uses the default address `192.168.4.1`:

```c
esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
```

Then initialize and start Wi-Fi:

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

If you do not need a custom address pool, you do not need to start another DHCP server. If AI Passport re-creates the default AP netif or re-starts DHCP when re-entering the provisioning page, the common result is a handle leak, state conflict, or a crash on re-entry.

### 3.2 To change the AP address, stop DHCP first, then change

```c
esp_netif_ip_info_t ip = {0};
IP4_ADDR(&ip.ip,      192, 168, 4, 1);
IP4_ADDR(&ip.gw,      192, 168, 4, 1);
IP4_ADDR(&ip.netmask, 255, 255, 255, 0);

ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip));
ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
```

IP, gateway, DNS redirect target, HTTP redirect address, and DHCP Option 114 must all use the same AP address. Change one and miss the others, and you get "phone got an address but the page will not open" or "popup opens then loops redirect".

### 3.3 How to confirm DHCP is working

At minimum the serial log records:

- SoftAP started successfully and the actual IP.
- `WIFI_EVENT_AP_STACONNECTED`: the phone connected to the hotspot.
- DHCP allocation log: the phone got an address like `192.168.4.2`.
- The HTTP root path received a request.

If the phone shows "getting IP" then disconnects, check the DHCP and the AP netif first, not the HTML page. The page has no chance to participate in that stage yet.

## 4. STA connection and how to write the DHCP client

The default STA netif carries a DHCP client. After setting the home Wi-Fi, call `esp_wifi_connect()`, and treat `IP_EVENT_STA_GOT_IP` as the real success mark:

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

Do not treat `WIFI_EVENT_STA_CONNECTED` as done. It only means associated with the router; it does not prove IP, gateway, and DNS were obtained.

Wait with a timeout, for example 20 seconds; on timeout show a clear reason and allow re-submit. Do not reconnect in an endless loop that makes the provisioning page and the main task unresponsive.

### An easy-to-hit network address mistake

When testing on the device hotspot, the computer or phone providing a local service may be `192.168.4.2` while the device is `192.168.4.1`. But after the device switches to the home Wi-Fi, `192.168.4.2` is usually no longer the original server.

If the device later needs to reach a LAN service, the configured address must use the server's address on the home LAN, for example:

```text
http://192.168.5.18:18080
```

The firmware should reject service addresses that point into its own temporary SoftAP segment in the formal config, and prompt the user for a LAN address or domain that stays reachable after the switch. This validation avoids the "works in the provisioning page, then fails forever after the switch" problem.

## 5. How to auto-open the auth page

The Espressif official captive portal example uses two methods that can be enabled together: DNS/HTTP funneling, and DHCP Option 114. Reference: [ESP-IDF Captive Portal example](https://github.com/espressif/esp-idf/tree/master/examples/protocols/http_server/captive_portal) and [ESP-IDF HTTP Server docs](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c3/api-reference/protocols/esp_http_server.html).

### 5.1 DNS: answer all domains with the SoftAP IP

Start a DNS service on UDP port 53 and return the AP address for all A queries:

```c
dns_server_config_t dns =
    DNS_SERVER_CONFIG_SINGLE("*", "WIFI_AP_DEF");
dns_server_handle_t dns_handle = start_dns_server(&dns);
if (dns_handle == NULL) {
    ESP_LOGE(TAG, "DNS server start failed");
    return ESP_FAIL;
}
```

`dns_server` is not a system API you can assume exists. When porting, reference and bring in the DNS server component and CMake config from the ESP-IDF official captive portal example directly.

The DNS service must stop when leaving provisioning; otherwise re-entering the page can produce a port-53 conflict, a task leak, or access to an old netif handle.

### 5.2 HTTP: catch the system probe paths and unknown paths

Common probes include:

- Android: `/generate_204`
- Apple: `/hotspot-detect.html`
- Windows: `/connecttest.txt`, `/ncsi.txt`

Do not register only `/` and `/save`. Register handlers for the probe paths and return a redirect for other HTTP 404s:

```c
static esp_err_t redirect_to_portal(httpd_req_t *req)
{
    httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    // Clients like iOS need a response body; an empty redirect may not pop the auth page.
    return httpd_resp_send(req,
                           "Redirect to the captive portal",
                           HTTPD_RESP_USE_STRLEN);
}

static esp_err_t not_found_handler(httpd_req_t *req, httpd_err_code_t err)
{
    return redirect_to_portal(req);
}
```

Auth probe behavior changes with system version; do not target one fixed Host. The most robust strategy is: while provisioning, funnel every ordinary HTTP request except static assets, the root page, and the save endpoint to the root page.

HTTPS cannot be transparently redirected this way because the cert domain does not match. Do not intercept 443 on the ESP32 to fake a target site; the Captive Portal only guides ordinary HTTP probes and the DHCP hint.

### 5.3 DHCP Option 114: declare the auth page directly

ESP-IDF 5.5 can provide the auth page URL in the DHCP Offer. The DHCP server must be stopped before changing the option, then restarted:

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

The corresponding capability must be enabled per the ESP-IDF official example, `CONFIG_ESP_ENABLE_DHCP_CAPTIVEPORTAL`. The URI storage must stay valid for the DHCP server lifetime, so use static or long-lived global memory and do not pass a temporary array on the function stack.

Option 114, DNS funneling, and HTTP probes can all be enabled together. Different phones support different things; the three together are more reliable than betting on one mechanism.

## 6. How to write the provisioning form to avoid buffer overflow

### 6.1 Set a hard cap on the total request length first

A small provisioning page can set the form limit to 1024 bytes first, then tune by the actual measured fields:

```c
#define PROV_FORM_MAX 1024

if (req->content_len <= 0 || req->content_len >= PROV_FORM_MAX) {
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "form too large");
    return ESP_FAIL;
}
```

Reserve room for the trailing `\0` at the boundary, so use `>= PROV_FORM_MAX` to reject.

### 6.2 `httpd_req_recv()` does not guarantee one read completes

Loop the read:

```c
char body[PROV_FORM_MAX] = {0};
int received = 0;

while (received < req->content_len) {
    int n = httpd_req_recv(req,
                           body + received,
                           req->content_len - received);
    if (n == HTTPD_SOCK_ERR_TIMEOUT) {
        continue;  // allow only a limited number of retries and record the timeout
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

Do not `recv(req, body, sizeof(body))` and parse directly, and do not trust the browser to send the complete form in one shot.

### 6.3 Limit all three lengths

The HTML `maxlength` only improves UX and is not a security boundary. Limit all three layers at once:

| Field | Protocol limit | C buffer suggestion |
| --- | --- | --- |
| SSID | at most 32 bytes | `char ssid[33]` |
| Wi-Fi password | at most 64 bytes | `char password[65]` |
| Service address | example at most 127 bytes | `char service_url[128]` |
| Binding code or token | example at most 95 bytes | `char pairing_token[96]` |

Also watch URL encoding: one byte may be written as `%XX`, so the HTTP body's encoded length is longer than the decoded value. The decode function must know the target capacity and return failure when the output does not fit; it must not silently truncate and continue registering.

Recommended interface:

```c
bool url_decode_checked(char *dst,
                        size_t dst_len,
                        const char *src,
                        size_t src_len);
```

After parsing, check again: required fields present, service address protocol valid, address not mistyped as the temporary SoftAP segment, strings fully terminated.

### 6.4 Do not put big structures on a small task stack

If a 1024-byte body, the form struct, HTTP parse temporaries, and a log-format buffer all go on the handler task stack, it easily forms a peak. You can:

- Keep the page short; do not embed large images or fonts.
- Use a bounded heap allocation for a large body and check for failure; free it right after.
- Reduce simultaneous sockets; provisioning usually needs only one or two connections.
- Return right after the HTTP callback validates and copies; hand the slow connection to a worker task.

"Bigger HTTP server stack" only solves stack shortage, not heap, socket, DMA, or LVGL shortage. First confirm which kind of memory the overflow is in.

## 7. Right timing for config save and registration

Recommend separating "pending config" from "official config":

1. After submit, save to a temporary structure first.
2. STA connects and gets an IP.
3. Call a health check, device binding, or other business validation interface.
4. On success, set `configured = true` and commit NVS once.
5. On next boot, enter the main app only when all required fields and the `configured` state are valid.

If you write NVS early for power-loss recovery, the boot logic must also reject the half-baked config with `configured = false`. Do not misjudge provisioning complete just because NVS has an SSID.

If the business needs device identity, each device should random-generate it on first boot; do not bake in a key shared by all devices in public firmware. A one-time binding code is only for first binding, not a long-term device auth key.

## 8. Common pitfalls of AI Passport using APSTA mode

- AP and STA share one Wi-Fi radio. After STA connects to the router, the SoftAP channel may follow the STA channel, and the phone briefly dropping is not necessarily DHCP corruption.
- The phone often auto-switches back to cellular or another Wi-Fi because the hotspot has "no internet". The provisioning page must clearly tell the user to stay connected.
- A PC firewall may block the local service port. The device getting a STA IP does not mean it can reach the target service.
- A service listening only on `127.0.0.1` cannot be reached by the device; it must listen on the LAN interface.
- AI Passport supports only 2.4 GHz Wi-Fi; same-name dual-band networks, enterprise-auth networks, and hidden SSIDs need separate validation.
- AI Passport's BLE, Wi-Fi, LVGL, and audio all use internal RAM; if BLE and audio are not needed in provisioning, stop or defer their init.
- In a Wi-Fi disconnect event only update state and schedule a retry; do not destroy netif directly or block in the event callback.
- When leaving the provisioning page, stop DNS, HTTP, Wi-Fi in reverse order, unregister event handlers, then destroy AP/STA netif; release each handle once.

## 9. Common fault-locating table

| Symptom | More likely cause | Focus |
| --- | --- | --- |
| Phone cannot connect to the hotspot | AP not started, wrong auth params, out of memory | Wi-Fi start return value, AP events, remaining heap |
| Stuck "getting IP" | DHCP server/netif state abnormal | whether the default AP netif is used, repeated stop/start |
| Got `192.168.4.x` but page will not open | HTTP not started, wrong port, address mismatch | visit `http://192.168.4.1`, HTTP logs |
| Manual open works but no auto-pop | no DNS funnel, probe route, or Option 114 | UDP 53, 404 redirect, DHCP Offer |
| Some phones pop, some do not | different system probe strategies or caching | DNS, Option 114, per-system probe URLs |
| 400 right after submit | body too large, missing field, URL decode failed | `content_len`, field encoded length, logs |
| Restart on submit | handler task stack or heap peak too high | body placement, HTTP stack, concurrent sockets |
| Main app cannot reach the service after config success | service address written as the temporary hotspot's `192.168.4.2` | change to a post-switch reachable LAN IP or domain |
| STA connected but request fails | no IP/DNS yet, firewall block | wait for `IP_EVENT_STA_GOT_IP`, port reachability |
| Crash on second entry to the provisioning page | DNS/HTTP/task/netif not fully released | exit order, zeroing handles, repeated event registration |

## 10. Recommended real-device test matrix

At least cover:

1. Android, iPhone, Windows connect once each; record whether they auto-pop.
2. Manually visit `192.168.4.1`, so there is still a fallback when auto-pop fails.
3. Correct password, wrong password, empty password, 32-byte SSID, 64-byte password.
4. Overlong form, missing field, illegal `%` encoding, and rapid repeated submits.
5. Service unreachable, wrong binding code, firewall block, and service restart.
6. Power loss during provisioning, confirming a half-baked config is not treated as success.
7. Enter and exit the provisioning page 20 times in a row; check that task count, sockets, netif, and remaining heap do not keep dropping.
8. After APSTA channel switch, whether provisioning and registration still complete.

## 11. Directly reusable conclusions

- AI Passport is a no-PSRAM ESP32-C3 device; the provisioning page must stay light and budget SoftAP, DNS, HTTP, LVGL and task stacks together.
- Use the 240×320 display to keep showing the hotspot name and `192.168.4.1`; do not bet the whole entry on the phone auto-popup.
- UP, DOWN, OK are GPIO0 ADC resistor-ladder buttons; a long press can trigger actions like clearing config, but heavy work must go to a background task.
- The default SoftAP netif already carries a DHCP server; do not build another one unless there is an address need.
- Wait for `IP_EVENT_STA_GOT_IP` to connect to the router, not just Wi-Fi connected.
- DHCP assigns addresses, not the auto-popup; the auto-popup needs DNS/HTTP funneling, ideally plus Option 114.
- The auto-popup is not 100% reliable; always keep `http://192.168.4.1` manual entry on the device screen.
- Limit the HTTP body, the encoded field, and the decoded target buffer together; overflow must error clearly.
- The HTTP callback does only short work; connecting and business validation go to background tasks.
- The LAN service address must stay reachable after the device switches to the target Wi-Fi; do not reuse the client address under the temporary hotspot.
- All network services need symmetric start/stop so provisioning can be safely re-entered.

## Route

This is general, upstream-benefiting hardware and design experience, so it is proposed back to the upstream `FoloToy/ai-passport` as a documentation PR.
