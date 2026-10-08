<p align="right">
  <a href="resource-stress.md">English</a> · <strong>简体中文</strong>
</p>

# 通用资源压力实验

在[应用 Demo 到真机 SOP](application-demo-to-device-acceptance.zh_CN.md)的资源验收阶段使用此可选工具。
[`tools/resource_stress.py`](../../../tools/resource_stress.py) 为应用定义的任务运行确定性单核负载模型。
游戏、阅读器、记录器等应用使用同一个引擎。应用适配器只提供任务启停轨迹和参数，不实现独立调度器。

## 运行与验收负载

```bash
python3 tests/test_resource_stress.py
python3 tools/resource_stress.py \
  --profile tests/fixtures/resource_stress/reader-profile.json \
  --trace tests/fixtures/resource_stress/reader-trace.json \
  --output /tmp/reader-resource-report.json
python3 tools/resource_stress.py \
  --profile tests/fixtures/resource_stress/recorder-profile.json \
  --trace tests/fixtures/resource_stress/recorder-trace.json \
  --output /tmp/recorder-resource-report.json
```

两个样例都是合成负载，不代表基线 Demo 的实测实现。阅读器包含显示与缓冲播放；记录器包含采样、存储和状态刷新，不依赖音频。
测试还覆盖单独的周期任务及两个独立缓冲流。另一个逐微秒运行的参考调度器，将 150 组固定种子的混合负载与事件驱动引擎对比。
回归项包括忙等、抢占、停顿、空／满缓冲、截止时间超限、取消、重入、内存失败以及拒绝不完整校准。

应用可以通过共享 C/Wasm 核心或其他可执行测试程序导出任务需求轨迹，再使用同一个引擎。
应用状态、素材、校准参数和适配器放在各自的 Fork 或分支中。
合成样例用于验证模型，不是上游外设 Demo 的实测接入；此类接入及真机校准仍未完成。

工具为可选项，不加入上游 `tools/validate.sh` 关卡。
按路径触发的 `resource-stress.yml` 仅在工具源码、测试、样例、指南或工作流变化时运行其主机回归。
它不认证设备性能，也不应设为所有变更都必须满足的检查。

## 参数与轨迹契约（schema 2）

参数文件包含 `tasks`、`memory`、可选的 `stalls` 和 `calibration`。
时间均使用整数微秒，取自真机测量或明确标注的假设；不能用主机执行速度推导设备任务成本。

| 任务字段 | 含义 |
| --- | --- |
| `id`、`priority`、`cpu_us`、`mode`、`limits` | 唯一名称、唯一固定优先级（越大越高）、每个作业的 CPU 成本、任务模式和验收阈值 |
| 周期任务：`period_us`、`transfer_us`、`wait` | 释放间隔、随后按实际时间推进的传输时长、`blocking` 或 `busy` 等待 |
| 缓冲任务：`refill_us`、`capacity_us`、`initial_buffer_us` | 每次作业完成补充的服务时长、缓冲容量和启用时明确配置的预填充 |
| 周期任务阈值 | `max_lateness_us`、`max_dropped_jobs` |
| 缓冲任务阈值 | `max_underrun_us`、`max_startup_us` |

周期任务最多保留一个未完成作业。CPU 工作或传输尚未结束时到达的新作业会丢弃，不隐含无限积压。
截止时间为释放时间加周期，包含传输耗时；退出或轨迹结束时仍未完成的超时工作也计入延迟。
阻塞传输释放 CPU，忙等按任务优先级竞争 CPU。CPU 停顿期间传输仍然推进。

缓冲任务补充持续消耗的缓冲，并在写入之间等待空位。用可服务时长而不是字节描述余量：PCM 需按实际采样率换算样本数。
必须明确初始预填充，补充最多到容量，消费者在启用时立即开始消耗。
启动延迟为首次生产完成的时间，满缓冲不能掩盖从未完成第一次写入的情况。
模型假设恒定消耗速率和整批写入，不能据此判断包延迟、可变消耗速率或队列溢出。

轨迹包含 `schema: 2`、`duration_us` 和有序的 `events`。每个事件有 `at_us` 和 `active`，后者是完整的启用任务 ID 集合。
第一个事件从零开始；空集合表示空闲；集合不变不会重置任务。停用取消未完成工作，重新启用立即释放作业并按声明恢复预填充。
边界时刻先结算完成，再应用事件，最后释放新作业。其他字段可保留应用状态、输入标识和源码／产物哈希。

`stalls` 包含有序且不重叠的 `at_us`／`duration_us` 区间，期间任何任务都得不到 CPU 服务。
输入上限为 32 个不同优先级任务、10,000 个事件／停顿、600 秒及 200,000 次估算作业释放。
不支持同优先级时间片轮转，遇到此输入会拒绝执行。

`memory` 提供 `available_bytes`、`largest_block_bytes`、`reserve_bytes` 和命名的额外 `allocations`，均基于同一次采样基线。
引擎只扣除新增分配，检查剩余保留空间，并将每个请求与采样时最大连续块比较。
这是预算估算，不是内存分配器：多个请求即使通过，也可能造成碎片。
引擎没有写死设备内存大小、显示格式或音频采样率。

## 报告、校准与边界

报告按任务提供完成／丢弃／取消次数、截止时间延迟、首次写入延迟、补充间隔、欠载时长及最小缓冲余量。
不适用的计数为零，未使用的缓冲最小值为 `null`。
全局指标包含 CPU 占用时长（含停顿和忙等）及剩余内存。CLI 报告记录模型、参数和轨迹哈希以便复现。
退出码：`0` 表示模型阈值通过，`1` 表示阈值或校准失败，`2` 表示输入或 I/O 错误。

`calibration` 记录 `status`（`synthetic`、`partial` 或 `measured`）、`source` 和 `assumptions`。
实测参数额外要求 `date`、`firmware_sha256`，且不能留有未测假设。
这些元数据检查不验证测量真实性：仍须按 SOP 审查证据，并对照独立真机样本。仅声明实测不构成独立校准。

需要校准关卡时，在同一命令加入 `--require-calibrated`。合成或部分测量输入即使满足模型时限，也会失败。
报告始终保持 `hardware_acceptance: NOT RUN`；合成／部分测量报告也保持 `calibrated_resource_acceptance: NOT RUN`。
正例和失败报告都应保留，并核对注入故障是否因为预期原因失败。

模型不运行 FreeRTOS、中断、共享总线争用、锁、SPI/I2S 驱动、缓存、未显式注入的 Flash 停顿或堆碎片。
模拟延迟不反馈到 C/Wasm 应用状态。故障恢复与清理须通过可执行应用测试；持续时序、实际输出质量及最终验收依赖真机。
