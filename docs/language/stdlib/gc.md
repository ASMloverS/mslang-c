# gc — 垃圾回收控制

```ms
import gc
```

## 概述

控制 mslang 追踪式分代 GC 的运行参数与触发时机。正常应用代码无需手动调用 GC；
本模块主要用于**性能调优**、**测试场景**和**嵌入式集成**。

**GC 架构简述：** mslang 使用分代 GC。年轻代采用半空间复制算法（minor
collection），老年代采用标记—清除—紧缩算法（major collection）。当分配量超过
阈值时 GC 自动运行。调用 `gc.collect()` 可强制触发完整收集。

## 常量与类型

### GCStats

`gc.stats()` 返回 `GCStats` 对象，具有以下只读属性：

| 属性 | 类型 | 说明 |
|---|---|---|
| `allocated` | `int` | 当前已分配字节数 |
| `collected` | `int` | 累计回收字节数 |
| `youngCollections` | `int` | minor collection 总次数 |
| `oldCollections` | `int` | major collection 总次数 |
| `liveObjects` | `int` | 当前存活对象数量 |
| `gcTimeMs` | `float` | GC 累计耗时（毫秒） |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `collect` | `gc.collect()` | 强制完整收集（minor + major） |
| `collectYoung` | `gc.collectYoung()` | 强制 minor 收集（仅年轻代） |
| `disable` | `gc.disable()` | 禁用自动 GC |
| `enable` | `gc.enable()` | 重新启用自动 GC |
| `isEnabled` | `gc.isEnabled() → bool` | 查询自动 GC 是否启用 |
| `stats` | `gc.stats() → GCStats` | 返回 GC 统计信息 |
| `setThreshold` | `gc.setThreshold(youngKb=4096, oldKb=65536)` | 设置触发阈值 |
| `getThreshold` | `gc.getThreshold() → (youngKb, oldKb)` | 查询当前阈值 |
| `freeze` | `gc.freeze()` | 将当前所有对象移入永久空间 |
| `getObjects` | `gc.getObjects() → list` | 返回所有被追踪对象的列表（调试用） |
| `getReferrers` | `gc.getReferrers(obj) → list` | 返回直接引用 `obj` 的对象列表（调试用） |
| `getCount` | `gc.getCount() → (young, old)` | 查询各代当前分配计数 |

## 详细语义

### gc.collect / gc.collectYoung

```
gc.collect()        // minor + major
gc.collectYoung()  // 仅 minor
```

强制触发 GC。`collect()` 先执行年轻代收集，再执行老年代收集；`collectYoung()`
仅处理年轻代（速度更快，适用于延迟敏感场景中的临时清理）。

两者均为同步调用——返回时收集已完成。

### gc.disable / gc.enable

```
gc.disable()
gc.enable()
gc.isEnabled() → bool
```

禁用自动 GC 后，分配量超过阈值时**不再**自动触发收集；手动调用 `gc.collect()`
仍然有效。通常用于基准测试（排除 GC 干扰）或批量分配前的临时禁用。

**注意：** 长时间禁用 GC 可能导致内存大量增长，务必在合适时机重新启用。

### gc.stats

```
gc.stats() → GCStats
```

返回当前 GC 统计快照。多次调用返回独立对象，不共享状态。

```ms
s := gc.stats()
fmt.println(s.allocated, "bytes allocated")
fmt.println(s.gcTimeMs, "ms spent in GC")
```

### gc.setThreshold / gc.getThreshold

```
gc.setThreshold(youngKb=4096, oldKb=65536)
gc.getThreshold() → (youngKb, oldKb)
```

- `youngKb`：年轻代分配量阈值（KB）。超出后触发 minor collection（默认 4096）。
- `oldKb`：老年代分配量阈值（KB）。超出后触发 major collection（默认 65536）。

减小阈值可更频繁地收集（更低内存峰值，更高 GC 开销）；增大阈值反之。
两者均须为正整数，否则抛 `ValueError`。

### gc.freeze

```
gc.freeze()
```

将**当前时刻**所有存活对象移入永久空间（permanent generation），这些对象不再
参与任何 GC 收集，生命周期与进程相同。

主要用途：在 `fork()` 前调用，使父进程的对象在子进程中以写时复制（CoW）方式
共享，避免 GC 导致页面脏化。常见于多进程 web 服务预热后的 prefork 模式。

`gc.freeze()` 调用后新分配的对象仍正常参与 GC。

### gc.getObjects

```
gc.getObjects() → list
```

返回当前被 GC 追踪的所有对象的列表（不包括已冻结对象）。

**仅用于调试**：调用开销高（需遍历 GC 堆），返回列表本身也会短暂增加内存占用。
生产代码中不应调用。

### gc.getReferrers

```
gc.getReferrers(obj) → list
```

返回直接持有对 `obj` 引用的对象列表。用于排查内存泄漏（对象意外存活）。

**仅用于调试**：同样开销较高。返回的列表可能包含 GC 内部数据结构；过滤时应
跳过非业务对象。

### gc.getCount

```
gc.getCount() → (young, old)
```

返回两个整数，分别为年轻代和老年代自上次收集以来的新分配对象计数。当计数超过
对应阈值时，自动 GC 将触发。

## 示例

```ms
import gc
import fmt

// 1. 基准测试：禁用 GC 排除干扰
gc.disable()
start := time.now()

result := []
for i in range(100000) {
    result.append(i * i)
}

elapsed := time.since(start)
gc.enable()
gc.collect()   // 手动清理累积对象

fmt.println("compute time:", elapsed)

// 2. 查看 GC 统计
s := gc.stats()
fmt.println("live objects:", s.liveObjects)
fmt.println("total gc time:", s.gcTimeMs, "ms")

// 3. 调整阈值（减少内存峰值）
gc.setThreshold(youngKb=1024, oldKb=8192)
thresholds := gc.getThreshold()
fmt.println("thresholds:", thresholds)   // (1024, 8192)

// 4. 强制 minor 收集（延迟敏感场景）
gc.collectYoung()

// 5. 排查内存泄漏（调试用）
suspect := {"key": "value"}
refs := gc.getReferrers(suspect)
fmt.println("referrer count:", len(refs))

// 6. prefork 前冻结（多进程服务）
// app.preload()       // 预热所有模块和对象
// gc.collect()        // 回收初始化垃圾
// gc.freeze()         // 冻结：子进程 fork 后不会触发 GC 脏页
// server.forkWorkers(8)
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `setThreshold` 的 `youngKb` 或 `oldKb` 不为正整数 |
