# P10-T121 并行清扫

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

利用 M:N 调度器的多 Worker 线程并行执行清扫阶段：将老代/中代对象链表分成多个段，各 Worker 并发清扫，减少清扫时间（线性正比于存活对象数，可通过并行加速）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T120 | 并发三色标记（清扫阶段入口） |
| P9-T112 | M:N 多 Worker |

---

## 实现要点

### 1. 并行清扫策略

```c
// 将老代对象链表分为 N 段（N = Worker 数），各 Worker 并发清扫一段
// 使用 work-stealing：清完自己段的 Worker 可以去偷其他 Worker 的段

typedef struct SweepRange {
  MsObject** start;    // 指向链表某段的头
  MsObject** end;      // 指向下一段的连接点
  volatile int claimed; // 0 = 可偷，1 = 已被认领
} SweepRange;

SweepRange gSweepRanges[MS_MAX_WORKERS];

void msSplitOldGenForParallelSweep(void) {
  uint32_t nWorkers = gWorkerCount;
  uint32_t objCount = gOldGen.count;
  uint32_t segSize  = (objCount + nWorkers - 1) / nWorkers;

  MsObject** p = &gOldGen.allObjects;
  for (uint32_t i = 0; i < nWorkers; i++) {
    gSweepRanges[i].start   = p;
    gSweepRanges[i].claimed = 0;
    // 推进 segSize 个节点
    for (uint32_t j = 0; j < segSize && *p; j++) p = &(*p)->gcNext;
    gSweepRanges[i].end = p;
  }
}
```

### 2. 单段清扫

```c
static void sweepRange(SweepRange* range) {
  MsObject** p = range->start;
  while (p != range->end && *p) {
    MsObject* obj = *p;
    if ((obj->gcFlags & 0x03) == GC_WHITE) {
      *p = obj->gcNext;
      if (obj->type->tpFree) obj->type->tpFree(obj);
      msFree(obj);
    } else {
      obj->gcFlags &= ~0x03;  // 重置为 WHITE
      p = &obj->gcNext;
    }
  }
}

// Worker 清扫入口（在 GC 阶段运行，而非正常调度器阶段）
static void workerSweep(MsWorker* w) {
  // 先清自己的段
  sweepRange(&gSweepRanges[w->id]);

  // 尝试偷其他段
  for (uint32_t i = 0; i < gWorkerCount; i++) {
    if (i == w->id) continue;
    int expected = 0;
    if (atomic_compare_exchange_strong(&gSweepRanges[i].claimed, &expected, 1)) {
      sweepRange(&gSweepRanges[i]);
    }
  }
}
```

### 3. 清扫阶段协调

```c
// GC 协调器在标记结束后：
void msTriggerParallelSweep(void) {
  msSplitOldGenForParallelSweep();

  // 通知所有 Worker 进入清扫模式
  gVM.gc.sweepPhase = true;
  msUnparkAllWorkers();

  // 等待所有 Worker 完成清扫（barrier）
  pthread_barrier_wait(&gVM.gc.sweepBarrier);

  gVM.gc.sweepPhase = false;
}
```

---

## 验收标准（checklist）

- [ ] N 个 Worker 并行清扫，速度接近线性扩展（2 Worker ≈ 2×）。
- [ ] 并行清扫后，所有白色对象被释放，无遗漏，无重复释放。
- [ ] Work-stealing 正常：某段 Worker 先完成后可偷其他段。
- [ ] 清扫完成后老代链表一致（无损坏指针）。

---

## 测试用例（C 单测）

```c
// tests/test_parallel_sweep.c
void testParallelSweepCorrectness(void) {
  // 创建 10M 老代对象（一半可达，一半不可达）
  // 触发 GC
  // 验证：可达对象全部存活，不可达对象全部释放
  uint32_t before = gOldGen.count;
  msMajorGC();
  uint32_t after = gOldGen.count;
  MS_ASSERT(after == before / 2);  // 一半被回收
}
```

---

## Benchmark

```c
// benchmarks/bench_sweep.c
// 比较 1-Worker 和 4-Worker 清扫时间
// 目标：4 Worker 清扫 10M 对象 < 1-Worker 的 30%（≈3.3×加速）
```

---

## 风险与边界

- **链表段分割的原子性**：`SweepRange.start/end` 是链表指针，不能并发修改；已认领（`claimed=1`）的段只由一个 Worker 操作，安全。
- **`tpFree` 并发安全**：若 `tpFree` 函数有副作用（如关闭文件描述符），需保证并发调用安全；初版所有 `tpFree` 只释放内存，无并发问题。
