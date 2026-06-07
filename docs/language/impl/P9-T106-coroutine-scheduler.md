# P9-T106 MsCoroutine + 单线程协作调度器（基线）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 mslang 并发的基线：协程对象（`MsCoroutineObj`）和单线程协作式调度器（`MsScheduler`）。所有协程在同一 OS 线程内通过主动让步（yield/await）切换，为后续 M:N 多线程调度（T112）提供演进基础。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T051 | MsThread + eval 循环 |
| P5-T068 | 调用约定（协程如同函数） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `concurrency.md` | §2 协程（Goroutine）数据结构 |
| `concurrency.md` | §5 调度器（M:N 调度） |

---

## 实现要点

### 1. MsCoroutine 结构

```c
typedef enum CoroState {
  CORO_RUNNABLE = 0,   // 就绪，在运行队列中
  CORO_RUNNING  = 1,   // 正在某 OS 线程上执行
  CORO_WAITING  = 2,   // 阻塞于 channel/future/IO
  CORO_DEAD     = 3,   // 执行完毕
} CoroState;

struct MsCoroutine {
  struct MsObject   head;       // GC 对象头（type = &msCoroutineType）
  struct MsThread   thread;     // VM 线程状态按值内联（ip、帧链头、异常等）
  CoroState         state;
  MsValue           result;     // 完成时的结果（供 await）
  MsValue           exception;  // 完成时的异常（若有）
  struct MsCoroutine* next;     // 运行队列链表
  struct MsWaitList*  waiters;  // 等待本 coroutine 完成的协程（await）
  uint64_t          id;
};
```

> **帧链所有权**：`thread.topFrame` 指向的每个 `MsFrame` 独立堆分配，地址在协程生命周期内稳定。goroutine 跨 Worker 迁移（work-stealing）时只需更新目标 Worker 的 TLS「当前协程」指针，无需移动帧数据本身。

```c
MsType msCoroutineType = {
  .name     = "coroutine",
  .traverse = coroutineTraverse,
  .destroy  = coroutineDestroy,
};
```

### 2. 单线程调度器

```c
typedef struct MsScheduler {
  struct MsCoroutine* ready;    // 就绪队列头（FIFO 链表）
  struct MsCoroutine* running;  // 当前运行的协程
  uint32_t            count;    // 协程总数
} MsScheduler;

MsScheduler gScheduler = {0};

// 将协程加入就绪队列
void msSchedEnqueue(struct MsCoroutine* coro) {
  if (!gScheduler.ready) { gScheduler.ready = coro; coro->next = NULL; }
  else {
    struct MsCoroutine* tail = gScheduler.ready;
    while (tail->next) { tail = tail->next; }
    tail->next = coro; coro->next = NULL;
  }
  gScheduler.count++;
}

// 从就绪队列取下一个协程（供 schedulerYield 使用）
struct MsCoroutine* schedPickNext(MsScheduler* sched) {
  struct MsCoroutine* next = sched->ready;
  if (next) sched->ready = next->next;
  return next;
}

// 运行调度器直到所有协程完成
void msSchedRun(MsThread* vm) {
  while (gScheduler.ready) {
    struct MsCoroutine* coro = schedPickNext(&gScheduler);
    gScheduler.running = coro;
    coro->state = CORO_RUNNING;
    restoreVmState(vm, coro);
    vmRun(vm);   // 运行直到协程主动让出或完成
    saveVmState(coro, vm);
    gScheduler.running = NULL;
  }
}
```

### 3. 协程切换（save/restore VM 状态）

切换不依赖 OS 上下文切换，无独立 OS 栈。每个协程的全部 VM 状态存储在内联的 `thread` 字段中；帧链各 `MsFrame` 在堆上独立分配，地址稳定。切换即保存当前协程的 `ip`/`topFrame`，恢复下一协程的 `ip`/`topFrame`，然后 `vmRun()` 从新协程的 `ip` 继续求值。

```c
static void saveVmState(struct MsCoroutine* coro, MsThread* vm) {
  coro->thread.ip        = vm->ip;
  coro->thread.topFrame  = vm->topFrame;
  coro->thread.exception = vm->exception;
}

static void restoreVmState(MsThread* vm, struct MsCoroutine* coro) {
  vm->ip        = coro->thread.ip;
  vm->topFrame  = coro->thread.topFrame;
  vm->exception = coro->thread.exception;
}

void schedulerYield(MsScheduler* sched, MsThread* vm, struct MsCoroutine* current) {
  current->state = CORO_WAITING;
  saveVmState(current, vm);
  struct MsCoroutine* next = schedPickNext(sched);
  if (next) {
    restoreVmState(vm, next);
    next->state = CORO_RUNNING;
    sched->running = next;
  }
  // 若无就绪协程，vmRun 退出，msSchedRun 继续外层循环
}
```

### 4. OP_YIELD 指令

```c
// 协程内遇到 yield 或安全点让步时调用 schedulerYield()
case OP_YIELD: {
  MsValue v = POP();  // yield 值（基线版本暂不传递给调用方）
  gScheduler.running->result = v;
  schedulerYield(&gScheduler, vm, gScheduler.running);
  // 恢复后继续执行下一条指令（send value 初版总为 nil）
  PUSH(MS_NIL_VAL);
  DISPATCH();
}
```

---

## 验收标准（checklist）

- [ ] `go func() { print("hello") }()` → 协程入队，调度器运行后打印。
- [ ] 多协程交替输出（yield 点处切换）。
- [ ] 协程内异常不影响其他协程（各自异常隔离）。
- [ ] 协程完成后 `state == CORO_DONE`。
- [ ] `type(coroutine)` → `"coroutine"`。

---

## 测试用例（.ms）

```ms
// 基础协程
go func() {
    print("A start")
    yield   // 让出
    print("A end")
}()

go func() {
    print("B start")
    yield
    print("B end")
}()

// 预期（协作调度，顺序取决于 FIFO 队列）：
// A start
// B start
// A end
// B end
```

---

## Benchmark

```ms
// benchmarks/bench_coro.ms
// 1M 次协程切换
n := 1_000_000
counter := 0
go func() {
    for i in range(n) {
        counter = counter + 1
        yield
    }
}()
// 目标：1M yield 切换 < 1s
```

---

## 风险与边界

- **无 OS 栈**：协程不分配独立 OS 栈；VM 帧链在堆上按需分配 `MsFrame`，无固定上限。深递归的内存压力由 GC 和帧分配器（T051/T068）负责，而非协程大小参数。
- **单线程基线**：本任务（T106）实现单 OS 线程协作调度，不含 work-stealing 或 OS 线程池；多线程 M:N 调度在 T112 中扩展，基本 save/restore 接口不变。
- **`yield` 关键字**：基线版本暴露 `yield` 让出语义；`await`（async/await 协议）在 T110/T111 引入，共用同一 `schedulerYield` 机制。
