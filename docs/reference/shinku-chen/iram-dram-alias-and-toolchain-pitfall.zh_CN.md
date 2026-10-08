<p align="right">
  <strong>简体中文</strong> · <a href="iram-dram-alias-and-toolchain-pitfall.md">English</a>
</p>

# 换了新工具链，固件就起不来了：IRAM 与 DRAM 是同一片 SRAM

本文记录在 **Pocket Intercom** v1.13 固件（2026-10-03）上一次「看起来像堆 bug、实际是
内存布局变化」的排查。结论是通用的：**ESP32-C3 上 IRAM 与 DRAM 是同一片物理 SRAM 的两个
地址**，所以任何应用都会遇到同样的事。

> **验证状态。** 段大小、符号地址与堆起点读自两个本地构建的 ELF（`objdump -h`、`nm`），
> 失败与修复各有一份真机启动日志。结论在一台设备、一份固件上测得；「IRAM 代码变胖 = 堆变少」
> 这一条来自链接脚本本身，与具体应用无关。

## 现象：一个怪到堆头上的启动循环

本地重编的固件每次启动都崩：

```
E (703) NimBLE: Failed to allocate memory for ble_store_config_vars
assert failed: ble_store_config_init ble_store_config.c:1227 (0)
```

失败的那次分配很小，而前一行日志还显示空着几十 KB；同一份源码用**仓库 pin 的**
工具链编出来却能正常启动。这种「矛盾」正是**布局变化**（而不是泄漏）的信号。

## 为什么 16 字节的改动能吃掉 4 KB 堆

比较同一份源码两次构建的段大小：

| 段 | IDF 5.5.5 + `esp-14.2.0_20260121` | IDF 5.5.3 + `esp-14.2.0_20251107` |
| --- | --- | --- |
| `.iram0.text`（常驻 IRAM 的代码） | 100,982 B | 96,908 B |
| `.dram0.dummy` | 101,376 B | 97,280 B |
| `.dram0.data` | 10,636 B | 10,620 B |
| `.dram0.bss` | 135,640 B | 135,584 B |
| `_heap_start` | `0x3fcbc770` | `0x3fcbb720` |

凭空多出 4 KB 的这个 DRAM 段根本不是数据。esp-idf 的
`esp32c3/sections.ld.in` 给它写了这样的注释：

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

所以 `.dram0.dummy` 是 **IRAM 镜像在 DRAM 地址空间里的影子**：IRAM 里的代码与 DRAM 里的
数据在总线上是两个地址、物理上是同一片 SRAM，链接器必须把 DRAM 的位置计数器推过 IRAM
已经占掉的那一段。`NOLOAD` 表示它不占 flash、也不多占一个字节 —— 它的**大小只是
「IRAM 里的代码占了多少共享 SRAM」**。

于是这颗芯片上：

- **IRAM 代码涨多少，堆就少多少**，一一对应。没有任何配置能把这 4 KB 找回来，只能把代码
  挪出 IRAM（或编得更小）。
- 新一点的 GCC 多吐出一点 IRAM 代码，就足以让原本能启动的固件起不来。这次新工具链多出
  **4,074 B** IRAM 代码，而设备运行时的最大连续空闲块只有约 **8.7 KB**，启动期那次分配
  直接失败。

## 工具链是跟着 IDF 版本走的

两次构建差在 IDF 补丁版本上，而 IDF 的 `tools.json` 决定用哪个编译器：5.5.3 选
`esp-14.2.0_20251107`、5.5.5 选 `esp-14.2.0_20260121`。所以**钉住 IDF 版本同时也就钉住了
工具链** —— 而在同一个小版本线内升补丁，仍然可能改变生成的代码，把堆挤掉。

## 刷机前怎么查（不用连设备）

```bash
# 段大小：IRAM 镜像与它在 DRAM 里的影子
riscv32-esp-elf-objdump -h build/<app>.elf | awk '$2 ~ /\.iram0\.text|\.dram0\.dummy|\.dram0\.data|\.dram0\.bss/'

# 堆将从哪里开始
riscv32-esp-elf-nm build/<app>.elf | grep -E " (_heap_start|_bss_end)$"
```

拿这几个数字与「你确认能启动的那次构建」对比。把 `_heap_start` 当作契约：它往后挪多少，
堆就少多少，而这台设备没有任何余量可以吸收。

## 实践规则

1. 钉死 CI 与已发布 release 用的**那个** IDF 版本，用同一套环境重建；不要用「能编过
   的更新补丁版本」。
2. 凡是在不同环境里编出的固件，刷机前先比 `.iram0.text` 与 `_heap_start`；差几 KB 就
   决定这台设备起不起得来。
3. 把每个 `IRAM_ATTR` 都当成在花堆（在这颗芯片上确实如此）：只放中断与「关 flash 缓存
   期间要跑」的代码。
4. 如果空闲堆看起来还健康、启动期分配却失败，先怀疑布局而不是堆：**先读段大小，
   再给分配器加探针**。

## 相关经验

- [让手机负责联网的 BLE 语音上行](ble-voice-uplink-and-pairing.zh_CN.md)
- [Pocket Intercom 归档](intercom/README.zh_CN.md)
