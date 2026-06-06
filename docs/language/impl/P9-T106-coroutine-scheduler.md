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
| `concurrency.md` | §1 协程与调度器 |

---

## 实现要点

### 1. MsCoroutineObj 结构

```c
typedef enum MsCoroState {
    CORO_CREATED   = 0,
    CORO_RUNNING   = 1,
    CORO_SUSPENDED = 2,
    CORO_DONE      = 3,
} MsCoroState;

typedef struct MsCoroutineObj {
    MsObject      header;
    MsCoroState   state;
    MsThread*     thread;     // 专属 MsThread（独立栈）
    MsValue       result;     // 最终返回值
    MsValue       exception;  // 协程内未捕获的异常
    struct MsCoroutineObj* next;  // 调度器队列链表
} MsCoroutineObj;

MsType msCoroutineType = {
    .name     = "coroutine",
    .tp_mark  = coroutineMark,
    .tp_free  = coroutineFree,
};
```

### 2. 单线程调度器

```c
typedef struct MsScheduler {
    MsCoroutineObj* ready;    // 就绪队列头（FIFO 链表）
    MsCoroutineObj* running;  // 当前运行的协程
    uint32_t        count;    // 协程总数
} MsScheduler;

MsScheduler gScheduler = {0};

// 将协程加入就绪队列
void msSchedEnqueue(MsCoroutineObj* coro) {
    // 尾插
    if (!gScheduler.ready) { gScheduler.ready = coro; coro->next = NULL; }
    else { /* 找尾节点 */ ...; tail->next = coro; coro->next = NULL; }
    gScheduler.count++;
}

// 运行调度器直到所有协程完成
void msSchedRun(void) {
    while (gScheduler.ready) {
        MsCoroutineObj* coro = gScheduler.ready;
        gScheduler.ready = coro->next;
        gScheduler.running = coro;
        coro->state = CORO_RUNNING;

        // 切换到协程的线程上下文并运行
        MsValue result = msCoroResume(coro);

        gScheduler.running = NULL;
        if (coro->state != CORO_DONE) {
            // 未完成（主动 yield 或等待 channel）：重新入队（channel 等待时不入队）
        }
    }
}
```

### 3. 协程切换（ucontext / setjmp-longjmp）

```c
// 初版：使用 ucontext_t（POSIX）或 Fiber（Windows）
// 每个协程分配独立栈（默认 256KB）

#if defined(_WIN32)
#include <windows.h>
typedef LPVOID MsCoroContext;
static VOID CALLBACK coroEntry(LPVOID arg) {
    MsCoroutineObj* coro = (MsCoroutineObj*)arg;
    coro->result = eval(coro->thread);
    coro->state = CORO_DONE;
    // 切回调度器（main fiber）
    SwitchToFiber(gScheduler.mainFiber);
}
#else
#include <ucontext.h>
typedef ucontext_t MsCoroContext;
#endif

MsValue msCoroResume(MsCoroutineObj* coro) {
    // 切换到 coro->ctx
#if defined(_WIN32)
    SwitchToFiber(coro->fiber);
#else
    swapcontext(&gScheduler.mainCtx, &coro->ctx);
#endif
    return coro->result;
}

void msCoroYield(void) {
    gScheduler.running->state = CORO_SUSPENDED;
    // 切回调度器
#if defined(_WIN32)
    SwitchToFiber(gScheduler.mainFiber);
#else
    swapcontext(&gScheduler.running->ctx, &gScheduler.mainCtx);
#endif
}
```

### 4. OP_YIELD 指令

```c
// 协程内遇到 yield（或 await）时调用 msCoroYield()
// 切回调度器后，调度器将该协程重新入队（或等待事件）
case OP_YIELD: {
    MsValue v = POP();  // yield 值
    gScheduler.running->yieldVal = v;
    msCoroYield();
    // 恢复后继续执行下一条指令
    PUSH(MS_NIL_VAL);   // send value（初版总为 nil）
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

- **栈大小**：默认 256KB/协程；可通过 `MSLANG_STACK_SIZE` 环境变量调整。
- **跨平台**：Windows 用 Fiber API（`CreateFiber`/`SwitchToFiber`）；POSIX 用 `ucontext_t`（`makecontext`/`swapcontext`）；macOS/ARM64 上 `ucontext_t` 需 `_XOPEN_SOURCE 600`。
