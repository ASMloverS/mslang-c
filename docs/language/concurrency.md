# mslang 并发模型

## 1. 设计哲学

mslang 并发基于**统一协程调度器**：`go`+channel 的结构化并发与 `async/await` 的异步编程风格共享同一绿色线程运行时，channel 和 Future 都是一等可等待对象。

```
goroutine (go)
async func (await)
     │           │
     └──── 统一调度器 (M:N scheduler) ────┘
               │
     OS 线程池（工作线程，Worker Threads）
```

---

## 2. 协程（Goroutine）

### 2.1 数据结构

```c
typedef enum {
    CORO_RUNNABLE,    // 就绪，在运行队列中
    CORO_RUNNING,     // 正在某 OS 线程上执行
    CORO_WAITING,     // 阻塞于 channel/future/IO
    CORO_DEAD,        // 执行完毕
} CoroState;

typedef struct MsCoroutine {
    MsThread     thread;       // VM 线程（帧栈、异常等）— MsCoroutine 按值拥有此字段；MsThread.coro 为非拥有的回指针
    CoroState    state;
    MsValue      result;       // 完成时的结果（供 await）
    MsValue      exception;    // 完成时的异常（若有）
    struct MsCoroutine *next;  // 运行队列链表
    MsWaitList  *waiters;      // 等待本 coroutine 完成的其他 coroutine（await）
    uint64_t     id;
} MsCoroutine;
```

### 2.2 `go` 语句

```ms
go func() {
    // 在新 goroutine 中并发执行
    doWork()
}()

go someFunc(arg1, arg2)
```

VM `GO` 指令：分配 `MsCoroutine`，将函数与参数绑定，加入调度器全局运行队列（`GRQ`）或对应工作线程本地队列（`LRQ`）。

### 2.3 生命周期

goroutine 完成时：
1. 将 `state` 设为 `CORO_DEAD`，存储结果/异常。
2. 通知所有在 `waiters` 列表上等待的协程（`await goroutine`）。
3. GC 在其栈帧不再被引用时回收。

---

## 3. Channel

### 3.1 数据结构

```c
typedef struct MsChan {
    MsObject  head;
    uint32_t  capacity;    // 0 = 无缓冲
    uint32_t  len;         // 当前缓冲元素数
    uint32_t  head_idx;    // 环形缓冲读指针
    MsValue  *buf;         // 环形缓冲区（capacity > 0 时分配）
    MsWaitQueue senders;   // 等待发送的协程队列
    MsWaitQueue receivers; // 等待接收的协程队列
    bool      closed;
    MsMutex   lock;        // OS 互斥锁（保护以上字段）
} MsChan;
```

### 3.2 无缓冲 channel（同步）

```ms
ch := make(chan)
```

- 发送方在无接收方时**阻塞**（CORO_WAITING），进入 `ch.senders` 队列。
- 接收方到来时，调度器直接将值从发送方传递给接收方，双方均恢复就绪。
- "汇合（rendezvous）"语义：发送与接收必须同时就绪。

### 3.3 有缓冲 channel

```ms
ch := make(chan, 16)
```

- 缓冲未满时，发送立即完成（不阻塞）。
- 缓冲为空时，接收阻塞。
- 缓冲满时，发送阻塞。

### 3.4 channel 操作

```ms
ch <- value        // 发送
value := <-ch      // 接收
value, ok := <-ch  // 接收 + closed 标志（ok=false 表示已关闭且空）
close(ch)          // 关闭（只能关闭一次，否则 panic）

// 迭代（直到 close）
for v in ch { process(v) }
```

### 3.5 `select`

```ms
select {
case ch1 <- x:
    // 发送成功
case y := <-ch2:
    // 接收到 y
case z, ok := <-ch3:
    // 接收到 z（或 ch3 关闭）
default:
    // 无 case 就绪时立即执行
}
```

实现：
1. `SELECT_BEGIN` 指令锁定所有涉及 channel 的锁（按固定顺序避免死锁）。
2. 扫描所有 case：若有已就绪的，随机选一个执行。
3. 若无就绪 case 且有 `default`，执行 `default`。
4. 否则，将当前协程注册到所有 channel 的等待队列，挂起（`CORO_WAITING`）。
5. 任一 case 就绪时，调度器唤醒协程，解注册其他等待，执行对应分支。

---

## 4. async/await

### 4.1 `async func`

```ms
async func doSomething(x) {
    result := await someOtherAsync(x)
    return result * 2
}
```

调用 `async func` 时：
- **不立即执行**函数体。
- 创建 `MsCoroutine`（状态 `CORO_RUNNABLE`），加入调度器。
- 返回 `MsFuture`（对该 coroutine 的引用）。

```c
typedef struct MsFuture {
    MsObject   head;
    MsCoroutine *coro;    // 关联的 coroutine（或已完成时为 NULL）
    MsValue    result;    // 完成后的结果
    bool       done;
    MsWaitQueue waiters;  // await 本 future 的协程
} MsFuture;
```

### 4.2 `await`

```ms
result := await expr
```

`await` 的操作数（`awaitable`）可以是：
- `MsFuture`：等待 async func 完成。
- `MsChan`（接收）：等待 channel 有值。
- 实现了 `__await__(self)` 魔术方法的任意对象。

`AWAIT` 指令流程：
```
1. 弹出 awaitable
2. 若 awaitable 是 MsFuture 且 future.done：直接压入 future.result，继续
3. 否则：
   a. 将当前协程加入 awaitable.waiters
   b. 当前协程 state = CORO_WAITING（挂起）
   c. 调度器切换到下一个就绪协程
4. awaitable 完成后：
   a. 调度器将等待协程重新加入运行队列
   b. 恢复时 AWAIT 指令压入结果，继续执行
```

### 4.3 `await` 的限制

- `await` 只能在 `async func` 内部使用，在同步函数中使用是**编译期错误**。
- 顶层脚本可通过 `ms_run_async()` 启动入口 `async func`。

### 4.4 函数无颜色污染的考量

mslang 选择了**显式 `async func`** 而非无颜色方案，原因：
- 编译期可识别哪些调用点可能挂起，利于优化。
- 调用方明确知道是否获取 Future，接口清晰。
- 与 `go` 语句正交：`go asyncFunc()` 会在新 goroutine 中启动 async func 并立即返回 Future，而不等待。

---

## 5. 调度器（Scheduler）

### 5.1 M:N 调度

```
goroutine G1, G2, G3, ... (N 个)
         │
调度器（work-stealing 双端队列）
         │
OS 线程 M1, M2, ... (M 个，M = CPU 核数或配置值)
```

- 每个 OS 线程（Worker）有**本地运行队列（LRQ）**，LIFO 顺序（对缓存友好）。
- 全局运行队列（GRQ）用于负载均衡，FIFO 顺序。
- **工作窃取（work stealing）**：本地队列空时，随机从其他 Worker 的 LRQ 尾部偷取。

### 5.2 协作式抢占

当前版本为**协作式**：goroutine 在安全点主动让出（channel 阻塞、`await` 挂起、`CALL` 前检查标志）。

> **初版注意**：无回边的纯计算 goroutine（如 `for { heavy_compute() }`）不经过安全点，会阻塞 GC STW 与调度器公平性，使其他 goroutine 饥饿。初版需由用户保证 goroutine 周期性经过安全点（或调用 `go`/`await` 主动切换）。

后续版本可加入**基于信号的抢占**（类 Go 1.14 异步抢占），在 goroutine 长时间不经过安全点时强制切换。

### 5.3 goroutine 切换

切换不涉及 OS 上下文切换（同一 OS 线程）：

```c
void scheduler_yield(MsScheduler *sched, MsCoroutine *current) {
    current->state = CORO_WAITING; // 或 RUNNABLE
    save_vm_state(current);        // 保存 ip/frame 到 coro->thread
    MsCoroutine *next = pick_next(sched);
    restore_vm_state(next);        // 恢复 ip/frame
    next->state = CORO_RUNNING;
    // 返回到 vm_run() 的求值循环，从新 goroutine 的 ip 继续
}
```

每个 goroutine 的 VM 状态（帧链、栈）存在 `MsCoroutine.thread` 中，切换即切换这套指针。

### 5.4 goroutine 局部存储

通过 OS 线程的 thread-local 存储（`__thread` / `_Thread_local`）持有"当前运行的协程"指针，VM 求值循环通过此获取当前 `MsThread*`。

---

## 6. 并发安全

- mslang **没有 GIL**（全局解释器锁），多个 goroutine 真正并行于多个 OS 线程。
- **共享可变状态**（如同一 list/map 被多个 goroutine 访问）是**未定义行为**——用户需通过 channel 或 `sync` 模块（Mutex/RWMutex）保护。
- `sync.Mutex`、`sync.RWMutex`、`sync.WaitGroup`、`sync.Once` 由标准库提供（底层使用 OS 原语）。

---

## 7. GC 与并发的协作

- GC 暂停（STW）时，调度器通过 `safepoint_requested` 标志通知所有 OS 线程上的 goroutine 停止。
- 各 Worker 在安全点发现标志后，将当前 goroutine 挂起，向 GC 报告根（精确栈帧），进入 stop-the-world 屏障。
- 所有 Worker 到达后，GC 执行 STW 阶段（根标记/GC 根快照），之后释放屏障。
- 并发标记/清扫阶段，各 Worker 继续执行 goroutine（写屏障激活）。

---

## 8. 示例

### 8.1 生产者-消费者（channel）

```ms
func producer(ch) {
    for i := 0; i < 10; i++ {
        ch <- i
    }
    close(ch)
}

func consumer(ch) {
    for v in ch {
        print("recv:", v)
    }
}

ch := make(chan, 4)
go producer(ch)
go consumer(ch)
```

### 8.2 async/await 链式调用

```ms
async func readFile(path) {
    return await io.readFile(path)
}

async func processAll(paths) {
    results := []
    for p in paths {
        data := await readFile(p)
        results.append(data)
    }
    return results
}

async func main() {
    all := await processAll(["a.txt", "b.txt"])
    print(all)
}
```

### 8.3 select 超时模式

```ms
import time

async func withTimeout(fut, secs) {
    timer := time.after(secs)
    select {
    case result := <-fut:
        return result
    case <-timer:
        raise TimeoutError("timed out")
    }
}
```
