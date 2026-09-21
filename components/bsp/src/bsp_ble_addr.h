// 6 字节 BLE 地址的确定性比较;不依赖 NimBLE 或 ESP-IDF,便于 host test 覆盖。
// 私有头文件,只在 components/bsp 内部使用。
#pragma once

#include <stdbool.h>
#include <stdint.h>

// 比较两个 6 字节地址,按“扫描器里看到的字节顺序”(最高字节在前)。
// 地址按 ble_addr_t.val 的小端布局传入:val[0] 是最低字节,val[5] 是最高字节,
// 也就是扫描器显示的第一个字节。返回 <0 / 0 / >0。
//
// 不能用 memcmp():它从 val[0](最低字节)开始比,那是另一种字节序,在不少地址
// 对(例如 01:00:00:00:00:05 与 00:00:00:00:00:05)上会给出相反的大小结论。
int bsp_ble_addr_cmp(const uint8_t a[6], const uint8_t b[6]);

// 本机是否应该主动发起连接:地址大的一方主动连,另一方等对方连进来。
// 规则反对称 —— 真实设备上地址互不相等,因此两边算出的结果必然相反,
// 不会出现双方同时发起(相等地址只是定义上的兜底,双方都不发起)。
bool bsp_ble_addr_should_initiate(const uint8_t own[6], const uint8_t peer[6]);
