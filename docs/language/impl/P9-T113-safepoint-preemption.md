# P9-T113 安全点与抢占协作

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现两种协程让步机制：①**协作式**（已有，在 channel/await/yield 处显式切换）；②**安全点检查**（在热循环中插入 `OP_SAFEPOINT`，定期主动检查是否需要让步或 STW GC）。防止单个协程长时间占用 Worker，影响其他协程响应。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T112 | M:N 调度器 |
| P4-T051 | eval 循环 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `concurrency.md` | §5 调度器（抢占 / safepoint） |

---

## 实现要点

### 1. OP_SAFEPOINT 插入策略

```c
// 编译器在以下位置插入 OP_SAFEPOINT（可选）：
// - 每个循环的回边（loop back-edge）
// - 函数调用前（OP_CALL 已有隐式安全点）
//
// 实现：OP_LOOP 本身已是安全点候选；无需额外指令
// 可用计数器方案：每执行 N 条指令检查一次（不修改字节码）

// VM 中的全局计数器：
static volatile uint32_t gSafepointRequest = 0;
// GC/抢占请求时设为 1；Worker 在每 1000 条指令检查一次

// eval 循环顶部（或 OP_LOOP 处）：
#define CHECK_SAFEPOINT() \
  if (--t->yieldCounter <= 0) { \
    t->yieldCounter = YIELD_INTERVAL; \
    if (gSafepointRequest) msHandleSafepoint(t); \
  }
```

### 2. GC STW 安全点

```c
// GC 主线程设置 gSafepointRequest = SAFEPOINT_GC
// 每个 Worker 到达安全点后：

void msHandleSafepoint(MsThread* t) {
  MsWorker* w = msCurrentWorker();
  w->inSafepoint = true;

  // 等待 GC 或其他 Worker 完成请求
  msWorkerWaitForSafepointRelease(w);

  w->inSafepoint = false;
}
```

### 3. 时间片抢占（软件信号）

```c
// 定时器触发（每 10ms）：设置 gSafepointRequest = SAFEPOINT_PREEMPT
// Worker 到达安全点后 yield，让其他协程运行
//
// 实现：单独的定时器线程 + 写 volatile 标志（无信号，无 OS 中断）
static void* timerThread(void* arg) {
  while (!gVM.shutdown) {
    msSleepmMs(10);
    atomic_store(&gSafepointRequest, SAFEPOINT_PREEMPT);
  }
  return NULL;
}

void msHandleSafepoint(MsThread* t) {
  uint32_t req = atomic_load(&gSafepointRequest);
  if (req & SAFEPOINT_GC)      { }  // 参与 STW
  if (req & SAFEPOINT_PREEMPT) {
    // 将当前协程放回就绪队列头（让其他协程先运行）
    MsCoroutineObj* cur = gScheduler.running;
    cur->state = CORO_SUSPENDED;
    msSchedEnqueue(cur);
    msCoroYield();
  }
  atomic_compare_exchange_strong(&gSafepointRequest, &req, 0);
}
```

### 4. YIELD_INTERVAL 调优

```c
// 默认：每 1000 条字节码指令检查一次
// 可通过 MSLANG_YIELD_INTERVAL 环境变量覆盖
#define YIELD_INTERVAL 1000
```

---

## 验收标准（checklist）

- [ ] 无限循环的协程不会饿死其他协程（10ms 后被抢占）。
- [ ] GC STW 时所有 Worker 在 1ms 内到达安全点。
- [ ] 安全点检查开销 < 1%（每 1000 条指令一次条件分支）。
- [ ] `MSLANG_YIELD_INTERVAL=100` 生效（更频繁检查）。

---

## 测试用例（.ms）

```ms
// 测试抢占：A 和 B 两个协程，A 是 CPU 密集型
go func() {
    // 无 yield 的密集循环（会触发安全点抢占）
    s := 0
    for i in range(10_000_000) { s = s + i }
    print("A done:", s)
}()

go func() {
    print("B runs!")  // 即使 A 在运行，B 也能得到执行
}()
// 期望：B 的 print 在 A 完成前出现
```

---

## Benchmark

```ms
// 安全点检查开销
n := 100_000_000
t0 := time.now()
s := 0
for i in range(n) { s = s + i }
t1 := time.now()
print("100M loop:", t1-t0, "ms")
// 与无安全点版本对比，开销 < 1%
```

---

## 风险与边界

- **volatile vs 原子操作**：`gSafepointRequest` 需要 `_Atomic uint32_t`（C11）以防止 CPU 乱序；纯 `volatile` 不足以保证跨 CPU 可见性。
- **Mac/Windows 定时器精度**：Windows 最小定时精度约 15ms（可用 `timeBeginPeriod(1)` 提升到 1ms）；macOS 用 `pthread_cond_timedwait`。
