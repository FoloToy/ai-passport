<p align="right">
  <a href="lvgl-pool-budget-without-psram.md">English</a> · <strong>简体中文</strong>
</p>

# ESP32-C3（无 PSRAM）上的 LVGL 内存池预算

在 **《去远方》** 发布首个版本（`v1.0.0-faraway`，上游基线 `1051209`）后整理。这个应用常年驻留
约 80~110 个 LVGL 对象，再叠一层 24 个粒子的天气图层，于是真正决定「这一页能不能建起来」的资源
不是系统堆，而是 LVGL 那个静态池。任何界面规模到这个量级的 AI Passport 应用都用得上。

## 从开机起就把池子的水位打出来；不打印就只能猜

LVGL 的堆是一块独立的静态池，大小由 `CONFIG_LV_MEM_SIZE_KILOBYTES` 决定（本应用是 80），
而它和 WiFi、任务栈共用同一片 DRAM。ESP32-C3 没有 PSRAM 可以借，所以余量只能靠实测。
在每个值得关心的状态切换处打一行监控——开机，以及最大那张浮层的进出两侧：

```c
static void wx_mem_log(const char *why)
{
    lv_mem_monitor_t m;
    lv_mem_monitor(&m);
    ESP_LOGW("fa_wx", "%-12s LVGL free=%u biggest=%u frag=%u%% maxused=%u | sys=%u",
             why, (unsigned)m.free_size, (unsigned)m.free_biggest_size,
             (unsigned)m.frag_pct, (unsigned)m.max_used,
             (unsigned)esp_get_free_heap_size());
}
```

实机截取（为版面省略了部分行）：

```text
W (2590)  fa_wx: boot        LVGL free=8044 biggest=8044 frag=0% maxused=65608 | sys=131484
W (2828)  fa_wx: overlay-ON  LVGL free=8420 biggest=8060 frag=5% maxused=68096 | sys=131484
W (6299)  fa_wx: overlay-OFF LVGL free=8424 biggest=8060 frag=5% maxused=68492 | sys=131616
W (168727) fa_wx: overlay-OFF LVGL free=8052 biggest=7576 frag=6% maxused=68492 | sys=131484
```

## 每一栏怎么读——判余量看 `maxused`，不是看 `free`

- **`maxused`** 是开机至今的峰值，也是唯一能回答「离炸还有多远」的一栏。一个看起来还很空的池
  （`free` 还有几千字节），照样可能是下一张页面就把它压死的那一个。
- **能不能分配成功看 `free_biggest_size`，不是 `free_size`**：几块小碎片加起来可以凑出一个好看的
  `free`，却没有单块大到足以再放一个全屏变换缓冲。要看 `biggest`。
- **不要用 `free` 和 `maxused` 相加去倒推池总量。** 两者口径不同，算式不闭合——在上面这份实测里，
  `80 KB − free = 73876`，比 `maxused` 的 68492 还大，而峰值水位不可能是这种关系。
  把 `maxused` 当趋势看、把 `free_biggest_size` 当分配闸门看，池的大小直接读
  `CONFIG_LV_MEM_SIZE_KILOBYTES`，不要从监控行反推。
- **`sys`** 是 IDF 系统堆。调 `CONFIG_LV_MEM_SIZE_KILOBYTES` 就是在它和 WiFi/任务栈之间搬内存，
  抬高一边必然压低另一边，两个数必须一起看。

## 采样要够长，才分得清「峰值」和「泄漏」

单次读数分不出峰值和缓慢泄漏。在最重的那张页面的进出两侧反复取这一行、比较整段运行里的
`maxused`：本应用在开机 **6.3 秒**内就摸到 68492 B 的水位，此后 **162 秒、30 个采样点纹丝不动**，
同期 `free` 在 7900~8424 之间漂、`free_biggest_size` 在 7576~8092 之间漂。**一段长跑里
`maxused` 平直，才是没有泄漏的证据**；会漂的那个才是警报。把重页面建起来、退出去、再回来——
开发过程中光天气图层就值得新增 29 个常驻对象，后来的纪念品图鉴又加了 25 个。

## 池子用尽的症状是整屏定格，不是崩溃

这个失败特征值得记住，因为它看起来像死机，很容易错怪到显示驱动头上。当
`CONFIG_LV_MEM_POOL_EXPAND_SIZE_KILOBYTES` 为 0 时，LVGL 9 没有池扩容这回事：
`lv_malloc` 失败 → `LV_ASSERT_MALLOC` 触发默认 handler（`while(1);`）→ 渲染任务停在半路，
而背光早就亮了。结果是**一帧画到一半的定格画面：不重启、不 panic、也没有任何串口输出**
（因为 `LV_USE_LOG` 没开）。**如果一张页面一打开就定格、日志又是干净的，先怀疑池子，别怀疑屏。**

两条出路，代价不同：

- **调大 `CONFIG_LV_MEM_SIZE_KILOBYTES`**：改动最小、保留静态池的隔离保证，但这块内存直接从
  系统堆里挖走。
- **把 LVGL 换成 C 库分配器**（`CONFIG_LV_USE_CLIB_MALLOC=y`）：把固定池并进通用堆，天花板彻底
  消失，代价是失去渲染与 WiFi 分配之间的隔离。这条路见「音量计 UI」那篇经验条目。

## 给下一个应用的结论

- 从开机起就打 `lv_mem_monitor()`；没量过的池子就是没法做预算的池子。
- 余量看 `maxused`、能否分配看 `free_biggest_size`、池子大小看 sdkconfig——**永远不要靠监控行的
  几栏互相加减**。
- 判「安全」之前，先把最重的那页反复进出采样；只有长跑里 `maxused` 平直，才算排除了泄漏。
- 「定格在半帧 + 日志干净」在证伪之前一律当成池子耗尽，不是显示问题。
- 每张新页面的对象数是一次**内存决策**，不只是界面决策。

## 相关文档

- `docs/reference/starsms007/faraway/README.zh_CN.md` —— 这些实测数据来自哪个应用。
- `main/fa_view.c` —— `wx_mem_log()` 与 `build_scene()` 周边关于池子的推理。
- `sdkconfig.defaults` —— `CONFIG_LV_MEM_SIZE_KILOBYTES`，以及当初为什么调大。
- `docs/reference/y2lin/meter-ui-smoothing-and-layout.zh_CN.md` —— 同一件事的另一面：
  池子耗尽表现为开机白屏，以及 `CONFIG_LV_USE_CLIB_MALLOC` 这个解法。
- `docs/reference/shinku-chen/display-refresh-and-deep-sleep.zh_CN.md` —— 同一块无 PSRAM 目标板上的
  显示刷新行为。
