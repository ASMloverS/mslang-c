# P12-T192 stdlib: threading

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `threading` 模块（对齐 `stdlib/threading.md`）：高级线程接口，基于 M:N 调度器，提供 Python 风格 Thread/Event/Semaphore API。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T112 | M:N 调度器（Thread 映射到协程） |
| P12-T182 | sync（底层 Mutex/Cond） |

---

## API 清单

```ms
// Thread（高层，底层为协程）
t := threading.Thread(target=func, args=(), kwargs={}, name=nil, daemon=false)
t.start()             // 启动（go func(args)）
t.join(timeout=nil)   // 等待完成（协程阻塞直到 t 完成）
t.is_alive() → bool
t.name → str
t.daemon → bool       // 后台线程（主线程结束时不等待）

threading.current_thread() → Thread  // 当前协程对应的 Thread 对象
threading.main_thread() → Thread
threading.active_count() → int       // 活跃线程（协程）数
threading.enumerate() → list[Thread] // 所有活跃 Thread

// Thread 子类（重写 run）
class MyThread(threading.Thread):
    func run(self) {
        // ... 线程逻辑 ...
    }

// Event（通知一次性事件）
ev := threading.Event()
ev.set()              // 标记事件发生
ev.clear()            // 重置
ev.is_set() → bool
ev.wait(timeout=nil)  // 阻塞直到 set（或超时）

// Semaphore
sem := threading.Semaphore(n=1)  // n=1 等同 Mutex
sem.acquire(blocking=true, timeout=nil) → bool
sem.release()
sem.value → int   // 当前计数

threading.BoundedSemaphore(n)  // 不允许超过初始值 release

// Timer（延迟执行）
t := threading.Timer(interval, func, args=(), kwargs={})
t.start()    // interval 秒后执行 func
t.cancel()   // 取消（若尚未执行）

// Barrier（所有线程同步到同一屏障点）
b := threading.Barrier(n_parties, action=nil, timeout=nil)
b.wait(timeout=nil)    // 阻塞直到 n 个线程都到达
b.reset()
b.abort()
b.parties → int
b.n_waiting → int
b.broken → bool

// local()（线程局部存储）
mylocal := threading.local()
mylocal.data = "this is thread-local"
// 各协程看到各自独立的 mylocal.data
```

---

## 实现要点

```c
// Thread 对象：包裹协程（MsCoroutineObj）
// start()：go target(*args, **kwargs)；记录协程引用
// join(timeout)：
//   若 timeout=nil：wait until done channel 关闭
//   若 timeout>0：select done channel vs time.after(timeout)

typedef struct MsThreadObj {
    MsObject      header;
    MsCoroutineObj* coro;   // 底层协程
    char*          name;
    bool           daemon;
    MsChannelObj*  done;   // 完成信号（协程结束时关闭）
    MsValue        result;
    MsValue        exc;    // 协程中的异常（若有）
} MsThreadObj;

// Event：带 set 标志的 channel
// set() → close(done_ch)（让所有 wait() 解除阻塞）
// clear() → 创建新 done_ch（重置）
// wait(timeout)：select <-done_ch case <-time.after(timeout)

// threading.local()：
// 基于 MsThread.locals MsMapObj（每个协程独立）
// __get__/__set__：通过 msCurrentThread() 查找 locals dict

// Barrier：
// count: 到达数量，generation: 当代编号，broken: bool
// wait()：原子增 count；若 count == parties：触发 action + broadcast；否则等待 cond
// reset()：广播让所有等待中的协程以 BrokenBarrier 退出

// daemon thread：
// 主线程退出（main 函数返回）时：
//   等待所有 non-daemon 协程完成
//   然后强制取消（cancel）daemon 协程
```

---

## 验收标准（checklist）

- [ ] `Thread(target=f).start(); t.join()` 等待 f 完成。
- [ ] `Event.wait()` 阻塞直到 `Event.set()`。
- [ ] `Semaphore(3)` 最多 3 个协程同时在临界区。
- [ ] `threading.local()` 不同协程看到各自独立值。
- [ ] `Barrier(5).wait()` 等到 5 个协程都到达再继续。
- [ ] daemon thread：主线程完成后 daemon 自动结束。

---

## 测试用例（.ms）

```ms
import threading, time

// 基础 Thread
results := []
def worker(n) {
    time.sleep(0.01 * n)
    results.append(n)
}
threads := [threading.Thread(target=worker, args=(i,)) for i in range(5)]
for t in threads { t.start() }
for t in threads { t.join() }
print(sorted(results))  // [0,1,2,3,4]

// Event 同步
ev := threading.Event()
go func() {
    time.sleep(0.1)
    ev.set()
}()
ev.wait(timeout=1.0)
print(ev.is_set())  // true

// threading.local
local := threading.local()
wg := sync.WaitGroup()  // 假设已 import sync
wg.add(5)
for i in range(5) {
    go func(val) {
        local.val = val
        time.sleep(0.01)
        print(local.val == val)  // 每个协程看到自己设置的值
        wg.done()
    }(i)
}
wg.wait()

// Barrier
import sync
b := threading.Barrier(3)
reached := sync.AtomicInt(0)
for _ in range(3) {
    go func() {
        b.wait()
        reached.add(1)
    }()
}
time.sleep(0.1)
print(reached.load())  // 3（同时通过屏障）
```
