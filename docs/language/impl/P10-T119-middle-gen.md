# P10-T119 中代标记-清除 + 晋升

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现中代（Middle Generation）GC：中代对象经历过 Minor GC 晋升，使用标记-清除算法回收（短暂 STW，与 Minor GC 连续执行）。中代收集**独立于老代 Major GC**，由 Minor GC 后中代占用率超过阈值（默认 50%）触发；存活对象晋升老代。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T116 | Minor GC（晋升到中代的逻辑） |
| P10-T117 | 精确根枚举 |
| P10-T118 | 写屏障（跨代引用） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `gc.md` | §4.5 Middle GC（中代收集）触发条件与流程 |
| `gc.md` | §2 内存空间布局（中代分代策略，bit1-2=1） |

---

## 实现要点

### 1. 中代布局

```c
// 中代：链式分配（free list）
// 对象追踪通过外部 MsMidGenEntry 链表而非 gcNext 内联字段
typedef struct MsMidGen {
  size_t   bytesUsed;
  size_t   capacity;    // 中代总容量（默认 16MB）
  size_t   threshold;   // 触发 Middle GC 的阈值（= capacity * 50%）
} MsMidGen;

// threshold = capacity * 0.5（gc.md §4.5）
MsMidGen gMidGen = { .capacity = 16 * 1024 * 1024,
                     .threshold = 8 * 1024 * 1024 };
```

### 2. Middle GC 触发时机

```c
// 在每次 Minor GC 末尾检查（gc.md §4.5：中代占用超过 50% 阈值）：
void msMinorGC(void) {
  // ... Minor GC 逻辑 ...
  if (gMidGen.bytesUsed > gMidGen.threshold) {
    msMidGC();   // 触发中代 GC（独立于老代 Major GC）
  }
  // 老代 Major GC 由 msMidGC 内晋升失败或老代占用率 > 75% 触发（T120）
}
```

### 3. 中代 GC（Mark-Sweep，STW）

```c
// gc.md §4.5：中代 GC 独立于老代 Major GC
void msMidGC(void) {
  msStopAllWorkers();  // 短暂 STW，与 Minor GC 连续执行

  // 标记阶段：从根出发标记所有可达中代对象（gcFlags bit0 = MS_GC_MARK）
  msEnumerateRoots(markRootVisitor, NULL);
  // 同时枚举老代 → 中代的 remembered set（写屏障记录的跨代引用）
  msEnumerateOldToMidRemSet(markRootVisitor, NULL);
  while (!grayQueueEmpty()) {
    struct MsObject* obj = grayQueuePop();
    obj->type->traverse(obj, markVisit, NULL);
    obj->gcFlags |= MS_GC_MARK;   // 置 mark 位（bit 0）
  }

  // 清扫中代：释放未标记对象，晋升高龄存活对象到老代
  sweepMidGen();

  msResumeAllWorkers();
  gVM.gc.midCount++;
}

// 注：中代对象迭代通过中代分配器的外部跟踪结构（非 gcNext 内联字段）
void sweepMidGen(void) {
  // 遍历中代所有已分配对象（具体迭代方式由分配器提供，此处为伪代码）
  forEachMidGenObject(^(struct MsObject* obj) {
    if (!(obj->gcFlags & MS_GC_MARK)) {
      // 未标记：不可达，释放
      if (obj->type->destroy) obj->type->destroy(obj);
      midGenFree(obj);
      gMidGen.bytesUsed -= msObjSize(obj);
    } else {
      // 存活：清除标记，更新晋升年龄
      obj->gcFlags &= ~MS_GC_MARK;
      uint8_t age = (obj->gcFlags >> 4) & 0x3;
      if (age >= 3) {
        promoteToOld(obj);   // 晋升到老代
      } else {
        obj->gcFlags = (obj->gcFlags & ~(0x3 << 4)) | ((age + 1) << 4);
      }
    }
  });
}
```

---

## 验收标准（checklist）

- [ ] 经历 2 次以上 Minor GC 的对象晋升到中代链表。
- [ ] `msMajorGC` 后，中代中不可达对象被释放。
- [ ] 晋升年龄 >= 3 的中代对象移入老代（T120 的老代链表）。
- [ ] Major GC 后内存使用量正确减少（无内存泄漏）。
- [ ] Minor GC 后中代占用 > 50% 阈值时自动触发 Middle GC（独立于老代 Major GC）。

---

## 测试用例（.ms）

```ms
// 生命周期较长的对象最终晋升
import gc
gc.enable()

long_lived := []
for i in range(1000) {
    long_lived.append(str(i))  // 会经历 Minor GC 晋升
}
gc.collect()
print("survived:", len(long_lived))  // 1000（全部存活）
```

---

## Benchmark

```ms
// benchmarks/bench_major_gc.ms
// 长期存活对象 + 短期死亡对象混合
import time
survivors := []
t0 := time.now()
for i in range(10_000_000) {
    x := str(i)          // 短期对象（Minor GC 回收）
    if i % 100 == 0 {
        survivors.append(x)  // 长期对象（晋升）
    }
}
t1 := time.now()
print("mixed alloc:", t1-t0, "ms")
print("survivors:", len(survivors))
// 目标：< 5s（含多次 Minor + Major GC）
```

---

## 风险与边界

- **中代内存分配器**：中代使用 free list 而非 bump pointer（不能移动对象）；初版用系统 `malloc`/`free` + 链表，后续可引入 size-class 分配器。
- **Major GC 的 STW**：Major GC 目前仍是 STW；T120 将引入增量/并发老代 GC，减少停顿时间。
