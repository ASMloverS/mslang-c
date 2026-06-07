# P10-T120 老年代增量 / 并发三色标记 + Dijkstra 写屏障

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现老年代的增量/并发三色标记 GC：标记阶段与程序并发执行（mutator 继续运行），通过 Dijkstra 写屏障保证标记正确性（不遗漏新增引用）；清扫阶段可与 mutator 并发执行，减少停顿时间。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P10-T119 | 中代 GC（老代晋升来源） |
| P10-T118 | card table 写屏障（在此基础上叠加三色写屏障） |
| P9-T112 | M:N 调度器（并发 GC 需要 GC 线程） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `gc.md` | §6 并发三色标记 + Dijkstra 写屏障 |

---

## 实现要点

### 1. 三色标记状态

```c
// 在 MsObject.gcFlags 中：
// GC_WHITE = 0x00（未标记/可回收）
// GC_GRAY  = 0x01（已入灰色队列，待展开）
// GC_BLACK = 0x02（已标记，不再处理）

typedef struct MsGrayQueue {
  MsObject** buf;
  uint32_t   head, tail, cap;
  pthread_mutex_t lock;
} MsGrayQueue;

MsGrayQueue gGrayQueue;
```

### 2. Dijkstra 写屏障

```c
// Dijkstra invariant：黑色对象不能直接引用白色对象
// 当黑色对象 owner 的字段被写入 newVal 时：
// 若 newVal 是白色，将其变为灰色（加入灰色队列）

#define DIJKSTRA_WRITE_BARRIER(newVal) \
  do { \
    if (gVM.gc.incrementalPhase && MS_IS_OBJ(newVal)) { \
      MsObject* obj = MS_AS_OBJ(newVal); \
      if ((obj->gcFlags & 0x03) == GC_WHITE) { \
        obj->gcFlags = (obj->gcFlags & ~0x03) | GC_GRAY; \
        msGrayQueuePush(&gGrayQueue, obj); \
      } \
    } \
  } while (0)

// 在 OP_SET_ATTR / OP_SET_INDEX / msMapSet / msListSetItem 等写操作中调用
```

### 3. 增量标记（分批执行）

```c
// GC 线程（或在安全点时工作）：每次处理灰色队列中的一批对象
void msIncrementalMarkStep(uint32_t budget) {
  uint32_t work = 0;
  while (work < budget && !msGrayQueueEmpty()) {
    MsObject* obj = msGrayQueuePop(&gGrayQueue);
    // 展开：标记 obj 所有引用的子对象
    obj->type->tpMark(obj);  // tpMark 内部将白色子对象加入灰色队列
    obj->gcFlags = (obj->gcFlags & ~0x03) | GC_BLACK;
    work++;
  }
  // 灰色队列为空 → 标记完成，进入清扫阶段
  if (msGrayQueueEmpty()) {
    gVM.gc.incrementalPhase = false;
    gVM.gc.sweepPhase       = true;
  }
}

// 在 mutator 的 msHandleSafepoint 中调用（抢占式标记工作）：
void msHandleSafepoint(MsThread* t) {
  if (gVM.gc.incrementalPhase)
    msIncrementalMarkStep(MARK_STEP_BUDGET);  // 默认 256 个对象/次
  // ...
}
```

### 4. 并发清扫

```c
// 清扫阶段：GC 线程遍历老代链表，释放白色对象
// mutator 同时运行（新分配对象标记为黑色，避免被回收）
static void* gcSweepThread(void* arg) {
  MsObject** p = &gOldGen.allObjects;
  while (*p) {
    MsObject* obj = *p;
    // 并发扫描：需要内存屏障确保看到完整对象状态
    atomic_thread_fence(memory_order_acquire);
    if ((obj->gcFlags & 0x03) == GC_WHITE) {
      *p = obj->gcNext;
      if (obj->type->tpFree) obj->type->tpFree(obj);
      msFree(obj);
    } else {
      obj->gcFlags &= ~0x03;  // 重置为 WHITE（为下次 GC）
      p = &obj->gcNext;
    }
  }
  gVM.gc.sweepPhase = false;
  return NULL;
}
```

---

## 验收标准（checklist）

- [ ] 标记阶段程序可继续运行（非 STW），停顿 < 1ms/步。
- [ ] Dijkstra 写屏障：新引用的白色对象被灰化，不遗漏。
- [ ] 清扫阶段并发执行，不与 mutator 冲突。
- [ ] Major GC 总停顿时间（累积 STW）< 10ms（100M 老代对象）。
- [ ] 无对象过早被回收（通过大量压力测试验证）。

---

## 测试用例（.ms）

```ms
// 压力测试：大量长寿对象 + 并发修改
import gc
import time
gc.enable_incremental()

data := {}
for i in range(100_000) {
    data[str(i)] = [i, i*2, i*3]
}

// 触发增量 GC 同时修改数据（测试写屏障正确性）
go func() {
    for k in data {
        data[k] = data[k]  // 重写（触发 Dijkstra 写屏障）
    }
}()

gc.collect()
print("data size after gc:", len(data))  // 应仍为 100000
```

---

## Benchmark

```ms
// benchmarks/bench_old_gen.ms
// 测量 GC 停顿时间（最大单次停顿）
import time
import gc

max_pause := 0.0
for i in range(1000) {
    t0 := time.now_ns()
    // ... 分配操作 ...
    t1 := time.now_ns()
    pause := t1 - t0
    if pause > max_pause { max_pause = pause }
}
print("max pause:", max_pause, "ms")
// 目标：增量 GC 后最大停顿 < 5ms
```

---

## 风险与边界

- **SATB vs Dijkstra**：Dijkstra 写屏障（保护写后对象）简单但保守（可能保留部分浮动垃圾）；SATB（Snapshot-at-the-beginning）更激进但实现更复杂。初版使用 Dijkstra。
- **浮动垃圾**：并发标记期间产生的新垃圾（mutator 释放引用，GC 已标记为活）在本次 GC 不回收，下次 GC 回收——属于正常语义。
