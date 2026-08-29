<p align="right">
  <a href="softap-provisioning-and-resource-budget.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# SoftAP Provisioning and Resource Budgets on AI Passport

This note summarizes reusable experience from implementing SoftAP provisioning
and a local configuration page on AI Passport. The difficult part is not making
an access point appear; it is keeping DHCP, captive-portal traffic, HTTP forms,
uploads, and the rest of the firmware inside the ESP32-C3's no-PSRAM budget.

## Choose the operating model first

Two products that both use SoftAP can need very different designs:

| Model | Recommended behavior |
| --- | --- |
| Short provisioning session | Use APSTA, collect credentials, verify the STA connection, persist only after success, then stop the AP and web service. |
| Long-lived local management | Prefer AP-only unless upstream connectivity is required; expose a stable local URL and budget for repeated reconnects and browser probes. |

Keeping APSTA active permanently costs more heap and sockets and creates more
radio work. Do not keep it merely because it was convenient during development.

## Release optional subsystems before starting Wi-Fi

On one AI Passport-derived firmware, the largest free block before SoftAP was
about 13 KiB. Deferring audio initialization and releasing unused codec/I2S
resources raised the available contiguous block to about 31 KiB. The exact
numbers are firmware-specific, but the lesson is general: lazy initialization is
often more valuable than shaving a few bytes from every network buffer.

Before provisioning, stop or defer resources that are not needed on that page:
audio tasks, decoder state, large images, temporary JSON documents, and duplicate
screens. Log free heap and largest free block before Wi-Fi start, after AP start,
after HTTP start, and after a phone joins.

## DHCP and connection state

Create the default SoftAP network interface and let ESP-IDF own its DHCP server
unless the product needs a custom address plan. A conservative measured starting
point for a one-user configuration page is `max_connection = 1`; increase it
only after measuring heap and socket use.

Do not confuse Wi-Fi association with DHCP completion:

- `WIFI_EVENT_AP_STACONNECTED` means the station joined the access point.
- `IP_EVENT_AP_STAIPASSIGNED` means the DHCP server assigned an address.

Use the second event when the UI needs to report that the client is actually
ready to open the page. Log both events, the assigned address, disconnect reason,
and current heap. This separates authentication problems from DHCP and HTTP
problems.

## Captive portals are a compatibility feature

A captive portal usually combines wildcard DNS with HTTP redirects and optional
DHCP captive-portal metadata. Different Android, iOS, and desktop versions send
different probe URLs, so a portal that works on one phone is not yet verified.

For a short provisioning flow:

1. Answer DNS queries with the AP address.
2. Serve known connectivity-check paths or redirect them to the local page.
3. Consider DHCP Option 114 where supported, but verify its presence on packets
   and real devices before documenting it as deployed.
4. Stop DNS and HTTP cleanly when provisioning completes.

For a long-lived management AP, automatic pop-up behavior can become annoying.
A printed local URL or QR code plus limited redirects may be a better product
choice. Wildcard DNS also creates background traffic; parse bounds, rate limits,
and socket cleanup still apply.

## Bound every HTTP input

Small form handlers can overflow just as easily as file uploads. Before receiving
a form:

- reject `Content-Length` above the endpoint limit;
- reserve space for the terminating byte;
- loop until the declared body is received or a timeout occurs;
- treat zero, timeout, and disconnect as explicit failures;
- URL-decode only after the complete bounded body is available;
- validate decoded field lengths before storing them.

Never write Wi-Fi credentials to persistent storage until the station connection
has been tested. Keep the old working credentials until the new pair succeeds,
so a typo does not strand the device.

For larger files, receive fixed-size chunks instead of allocating the whole body.
One measured implementation used 1024-byte chunks. Write to a temporary target,
validate size and format, synchronize concurrent access, then replace the active
asset only after success. Delete or invalidate the temporary target on every
failure path.

## Keep the web server deliberately small

One measured single-client configuration service used three HTTP sockets, a
backlog of two, two receive retries, ten send retries, LRU purge, a 6144-byte
server task stack, and 2880-byte TCP send/receive windows. These values are a
starting reference, not universal defaults.

Reduce web assets before enlarging transport buffers:

- gzip static pages at build time;
- send large JSON or file listings in chunks;
- keep thumbnails small and limit how many are decoded at once;
- close failed requests immediately and release their context;
- avoid multiple simultaneous fetches from the page.

A browser may open several connections even for one visible page. Count sockets
used by DNS, HTTP, STA verification, telemetry, and any remaining audio service
together.

## Failure signatures

| Symptom | Check first |
| --- | --- |
| Phone joins but never opens the page | DHCP assignment event, DNS replies, probe URL handling, HTTP socket availability |
| Random reset when a form is submitted | declared length, terminator space, receive loop, task stack, largest free block |
| Upload stops partway | receive timeout, chunk write result, storage space, concurrent reader/writer |
| Works once, then fails after retry | leaked socket, DNS task, event handler, netif, or temporary buffer |
| AP start fails only after other features run | largest free block and optional subsystems still initialized |

## Verification checklist

- Test Android, iOS, Windows, and at least one client that does not show a portal
  automatically.
- Test wrong credentials, unavailable router, DHCP timeout, AP disconnect, HTTP
  timeout, oversized form, interrupted upload, and repeated retries.
- Verify DHCP Option 114 with packet capture or equivalent device evidence when
  it is part of the claimed behavior.
- Run repeated enter/exit cycles and confirm tasks, handlers, sockets, and netifs
  return to a stable count.
- Record free heap, largest free block, and stack high-water marks at each phase.

The main lesson is that provisioning is a temporary network product inside the
firmware. Give it explicit start/stop ownership and strict input limits, and test
DHCP and captive-portal behavior separately instead of treating “connected to
Wi-Fi” as proof that the whole flow works.
