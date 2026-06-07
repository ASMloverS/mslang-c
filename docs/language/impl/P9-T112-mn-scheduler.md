# P9-T112 调度器演进：M:N 多 Worker + work-stealing

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

将单线程协作式调度器（T106）演进为 M:N 调度器：M 个 OS 线程（Worker）运行 N 个协程（goroutine），每个 Worker 有本地就绪队列，全局队列作为负载均衡补充，Worker 间通过 work-stealing 实现动态负载均衡。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T106 ~ T111 | 单线程调度器 + channel + async |
| P10-T117 | 精确根枚举（GC 需要枚举所有 Worker 的根） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `concurrency.md` | §5 M:N 调度器 + work-stealing |

---

## 实现要点

### 1. Worker 线程

```c
#include <pthread.h>

typedef struct MsWorker {
  pthread_t     thread;
  uint32_t      id;
  MsRunQueue    localQ;   // 本地就绪队列（lock-free deque）
  MsCoroutineObj* current; // 当前运行的协程

  // GC 安全点
  volatile bool   inSafepoint;
  volatile bool   stopRequested;

  // 统计
  uint64_t        numSteals;
  uint64_t        numRuns;
} MsWorker;

// 全局 Worker 池
MsWorker gWorkers[MS_MAX_WORKERS];  // 默认 = CPU 核数
uint32_t gWorkerCount;

// 全局就绪队列（溢出 + 新协程）
MsGlobalQueue gGlobalQ;
```

### 2. Lock-free 工作窃取队列（Chase-Lev Deque）

```c
// Owner 从 bottom 取（push/pop）；Thief 从 top 窃取（steal）
typedef struct MsRunQueue {
  MsCoroutineObj** buf;   // 环形缓冲
  uint32_t         cap;   // 必须是 2 的幂
  volatile int64_t top;   // thieves 从这里取
  volatile int64_t bottom; // owner 从这里操作
} MsRunQueue;

void rqPush(MsRunQueue* q, MsCoroutineObj* c);     // owner push bottom
MsCoroutineObj* rqPop(MsRunQueue* q);              // owner pop bottom
MsCoroutineObj* rqSteal(MsRunQueue* q);            // thief steal top
```

### 3. Work-Stealing 循环

```c
static void* workerMain(void* arg) {
  MsWorker* w = (MsWorker*)arg;
  msInitWorkerGC(w);  // 初始化 GC 本地状态（TLAB）

  while (!gVM.shutdown) {
    MsCoroutineObj* coro = rqPop(&w->localQ);

    if (!coro) coro = msGlobalQDequeue();   // 从全局队列取

    if (!coro) {
      // work-steal：随机选一个 Worker 偷
      uint32_t victim = msRandUint() % gWorkerCount;
      if (victim != w->id)
        coro = rqSteal(&gWorkers[victim].localQ);
    }

    if (!coro) {
      // 真正空闲：进入 park 状态（等待条件变量）
      msWorkerPark(w);
      continue;
    }

    // 运行协程
    w->current = coro;
    w->numRuns++;
    msCoroResumeOnWorker(w, coro);
    w->current = NULL;
  }
  return NULL;
}
```

### 4. 调度器接口变化

```c
// 原来的 msSchedEnqueue → 根据当前 Worker 决定入本地队列或全局队列
void msSchedEnqueue(MsCoroutineObj* coro) {
  MsWorker* cur = msCurrentWorker();
  if (cur && rqLen(&cur->localQ) < 256) {
    rqPush(&cur->localQ, coro);
  } else {
    msGlobalQEnqueue(coro);
  }
  msWorkerUnpark();  // 通知空闲 Worker
}
```

### 5. GC + 安全点（与 T117 配合）

```c
// STW GC 时：master Worker 请求所有 Worker 停在安全点
// 每个 Worker 在 OP_SAFEPOINT（热循环中插入）或 yield 时检查 stopRequested
```

---

## 验收标准（checklist）

- [ ] `MSLANG_WORKERS=4 mslang run foo.ms` → 使用 4 个 OS 线程。
- [ ] 1000 个协程并行在 4 个 Worker 上执行，总时间约为单线程 1/4。
- [ ] Work-stealing：空闲 Worker 从忙 Worker 偷协程，避免饥饿。
- [ ] GC STW 时：所有 Worker 停在安全点后，GC 运行，再恢复。
- [ ] 无数据竞争（channel 操作加锁，GC 有写屏障）。

---

## 测试用例（.ms）

```ms
// 并行计算 PI（Monte Carlo）
n := 10_000_000
ch := make(chan, 10)

for i in range(4) {
    go func(tid) {
        count := 0
        for j in range(n // 4) {
            x := random.random()
            y := random.random()
            if x*x + y*y < 1.0 { count = count + 1 }
        }
        ch <- count
    }(i)
}

total := 0
for i in range(4) { total = total + (<-ch) }
pi := 4.0 * total / n
print("π ≈", pi)
// 期望 ~3.14（Monte Carlo 估计）
// 目标：4 Worker 时比单线程快 ~3-4×
```

---

## Benchmark

```ms
// benchmarks/bench_mn_sched.ms
// CPU 密集型：4 Worker 并行
import time
n_coroutines := 100
n_work := 1_000_000

t0 := time.now()
ch := make(chan, n_coroutines)
for i in range(n_coroutines) {
    go func() {
        s := 0
        for j in range(n_work) { s = s + j }
        ch <- s
    }()
}
total := 0
for i in range(n_coroutines) { total = total + (<-ch) }
t1 := time.now()
print("100 × 1M ops:", t1-t0, "ms")
// 目标（4 Worker）: 比单线程快 3-4×
```

---

## 风险与边界

- **跨线程 GC 写屏障**：M:N 后，多个 Worker 同时写对象，需 Dijkstra 写屏障（P10-T120）；T112 先实现调度器骨架，GC 暂时仍 STW。
- **channel 锁**：无缓冲 channel 的 waiter 链表在多线程下需要 `pthread_mutex_t` 保护；这是 T112 引入多线程后必须加锁的第一批结构。
