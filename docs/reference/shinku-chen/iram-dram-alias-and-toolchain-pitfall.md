<p align="right">
  <a href="iram-dram-alias-and-toolchain-pitfall.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# When a Newer Toolchain Makes the Firmware Unbootable: IRAM and DRAM Share One SRAM

Captured after the **Pocket Intercom** v1.13 firmware build (2026-10-03) went into
a boot loop that looked like a heap bug and turned out to be a toolchain-driven
memory-layout change. The finding is general: it applies to any application on
this board, because on the ESP32-C3 **IRAM and DRAM are two addresses for the
same physical SRAM**.

> **Verification status.** Section sizes, symbol addresses and the heap start were
> read from two locally built ELFs (`objdump -h`, `nm`) plus one device boot log
> showing the failure and one showing the fix. Measured on one board and one
> firmware; the direction of the effect (IRAM code growth costs heap) is a
> property of the linker script, not of this application.

## Symptom: a boot loop that blames the heap

A locally rebuilt firmware aborted every boot:

```
E (703) NimBLE: Failed to allocate memory for ble_store_config_vars
assert failed: ble_store_config_init ble_store_config.c:1227 (0)
```

The allocation that fails is small, the log one line earlier still reported
tens of kilobytes free, and the *same source* built with the pinned toolchain
booted fine. That contradiction is the tell-tale sign of a **layout** change
rather than a leak.

## Why a 16-byte change can cost 4 KB of heap

Compare section sizes between the two builds of the same source:

| Section | IDF 5.5.5 + `esp-14.2.0_20260121` | IDF 5.5.3 + `esp-14.2.0_20251107` |
| --- | --- | --- |
| `.iram0.text` (code residing in IRAM) | 100,982 B | 96,908 B |
| `.dram0.dummy` | 101,376 B | 97,280 B |
| `.dram0.data` | 10,636 B | 10,620 B |
| `.dram0.bss` | 135,640 B | 135,584 B |
| `_heap_start` | `0x3fcbc770` | `0x3fcbb720` |

The DRAM section that grew by a full 4 KB is not data at all. `esp-idf`'s
`esp32c3/sections.ld.in` defines it with this comment:

```ld
/**
 * This section is required to skip .iram0.text area because iram0_0_seg and
 * dram0_0_seg reflect the same address space on different buses.
 */
.dram0.dummy (NOLOAD):
{
  . = ORIGIN(dram0_0_seg) + _iram_end - _iram_start;
} > dram0_0_seg
```

`.dram0.dummy` is therefore a **mirror of the IRAM image inside the DRAM address
space**: because code in IRAM and data in DRAM occupy the same physical SRAM
through two different bus addresses, the linker must advance the DRAM location
counter past whatever the IRAM image already occupies. `NOLOAD` means it costs
no flash and no extra bytes — its *size* is simply how much of the shared SRAM
the code in IRAM is using.

So on this part:

- **IRAM code growth is a direct, one-for-one loss of heap.** No configuration
  recovers that 4 KB; only moving code out of IRAM (or compiling it smaller) does.
- A newer GCC emitting slightly fatter IRAM code is enough to make a previously
  bootable firmware unbootable. Here the newer toolchain added 4,074 B of IRAM
  code — and the board's largest free block at runtime is only about 8.7 KB, so
  the boot-time allocation failed.

## The toolchain follows the IDF version

The two builds differed in IDF patch release, and IDF's `tools.json` decides
which compiler that release uses: 5.5.3 selects `esp-14.2.0_20251107`, 5.5.5
selects `esp-14.2.0_20260121`. Pinning the IDF version therefore also pins the
toolchain — and a patch-level jump inside the same minor line can still change
the code enough to move the heap.

## How to check before flashing (no device needed)

```bash
# section sizes, including the IRAM image and its DRAM mirror
riscv32-esp-elf-objdump -h build/<app>.elf | awk '$2 ~ /\.iram0\.text|\.dram0\.dummy|\.dram0\.data|\.dram0\.bss/'

# where the heap will start
riscv32-esp-elf-nm build/<app>.elf | grep -E " (_heap_start|_bss_end)$"
```

Compare these against the build you know boots. Treat `_heap_start` as the
contract: if it moves later, the heap shrank by the same amount, and this board
has no slack to absorb it.

## Practical rules

1. Pin the exact IDF release used by CI and the published release, and rebuild
   with that environment rather than "a newer patch that compiles".
2. Before flashing a firmware built in a different environment, compare
   `.iram0.text` and `_heap_start`. A difference of a few kilobytes decides
   whether the board boots.
3. Treat every `IRAM_ATTR` as spending heap, because on this part it does.
   Keep only interrupt and flash-disabled paths there.
4. If a boot-time allocation fails while free heap still looks healthy, suspect
   the layout, not the heap: read the section sizes before instrumenting
   allocators.

## Related experience

- [A BLE Voice Uplink Where the Phone Does the Networking](ble-voice-uplink-and-pairing.md)
- [Pocket Intercom](intercom/README.md)
