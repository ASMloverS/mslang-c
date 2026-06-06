# P10-T125 GC 压力测试 + benchmark（M6 里程碑）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

通过完整的 GC 压力 `.ms` 测试套件验证 P10 所有 GC 改进（T115–T124）：分代 GC 正确性、Minor/Major GC 时间、写屏障、终结器、大对象、并发标记正确性。此任务是 P10 阶段的**里程碑收口**（M6）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T115 ~ T124 | P10 所有任务 |

---

## M6 测试套件（`tests/ms/p10/`）

### `tests/ms/p10/gc_correctness.ms`

```ms
import gc

// 1. 基础存活验证：有根引用的对象不被回收
objs := [str(i) for i in range(10_000)]
gc.collect()
print(len(objs))   // 10000（全部存活）

// 2. 无根对象被回收
for i in range(100_000) { x := str(i) }  // 临时对象
before := gc.stats()["numObjects"]
gc.collect()
after := gc.stats()["numObjects"]
print(after < before)  // true（临时对象被回收）

// 3. 循环引用
a := {}
b := {}
a["b"] = b
b["a"] = a
del a, b
gc.collect()  // 循环引用（初版：可能不完全回收，取决于 GC 实现）
print("cycle gc done")
```

**期望输出**：
```
10000
true
cycle gc done
```

### `tests/ms/p10/generational_stress.ms`

```ms
import gc
import time

// 短命对象 + 长命对象混合
survivors := []
t0 := time.now()

for i in range(1_000_000) {
    temp := [i, i*2]  // 短命
    if i % 1000 == 0 {
        survivors.append(str(i))  // 长命
    }
}

t1 := time.now()
print("1M alloc with GC:", t1-t0, "ms")
// 目标 < 3s

gc.collect()
print("survivors:", len(survivors))  // 1000
```

### `tests/ms/p10/write_barrier_stress.ms`

```ms
import gc

// 写屏障正确性：跨代引用
old_list = []
for i in range(10_000) {
    old_list.append(str(i))
}

// 触发多次 Minor GC（让 old_list 晋升到老代）
for _ in range(20) {
    for i in range(10_000) { x := str(i) }  // 制造 Minor GC 压力
    gc.collect(0)  // 只 Minor GC
}

// 此时 old_list 应在老代；向其写入新的年轻代对象
new_val := str("new_young")
old_list.append(new_val)  // 触发写屏障：老代 → 年轻代引用

gc.collect(0)  // Minor GC：通过写屏障找到 new_val 并保留
print(old_list[-1])  // new_young（未被错误回收）
```

### `tests/ms/p10/finalizer_test.ms`

```ms
import gc
log := []

class Tracked {
    func __init__(self, id) { self.id = id }
    func __del__(self) { log.append(self.id) }
}

for i in range(100) {
    t := Tracked(i)
    del t   // 立即无根
}
gc.collect()
print(len(log))  // 100（所有终结器都调用了）
```

### `tests/ms/p10/large_object_stress.ms`

```ms
import gc
import time

t0 := time.now()
for i in range(100) {
    b := bytes(200_000)   // 200KB 大对象
    del b
    gc.collect()
}
t1 := time.now()
print("100 × large alloc/free:", t1-t0, "ms")
// 目标 < 200ms

// 大对象存活
big := bytes(500_000)  // 500KB
gc.collect()
print(len(big))   // 500000（存活）
```

---

## 验收标准（checklist）

- [ ] `gc_correctness.ms`：有根对象全部存活，无根对象被回收。
- [ ] `generational_stress.ms`：1M 混合分配 < 3s，GC 次数合理。
- [ ] `write_barrier_stress.ms`：跨代引用对象不被错误回收。
- [ ] `finalizer_test.ms`：100 个终结器全部调用。
- [ ] `large_object_stress.ms`：大对象分配/释放 < 200ms。
- [ ] Minor GC 停顿 < 5ms（4MB young gen）。
- [ ] Major GC 停顿 < 20ms（含增量模式，T120）。

---

## Benchmark（M6 综合）

```ms
// benchmarks/bench_gc_full.ms
import time
import gc

// 1. 分配速度（含 GC）
t0 := time.now()
for i in range(10_000_000) { x := [i] }
t1 := time.now()
print("10M allocs:", t1-t0, "ms")   // 目标 < 3s

// 2. GC 暂停统计
gc.stats() |> print

// 3. 长寿对象 + 短寿对象混合（内存稳定性）
survivors := []
for i in range(5_000_000) {
    temp := str(i)
    if i % 1000 == 0 { survivors.append(temp) }
}
print("stable survivors:", len(survivors))  // 5000
```

---

## 风险与边界

- **M6 定义**：M6 = 分代 GC 正确 + Minor GC < 5ms + 大对象可用 + 终结器可用 + 并发标记基础可工作。部分高级特性（NUMA、SATB、精确 GC 停顿 SLA）是后续优化点。
