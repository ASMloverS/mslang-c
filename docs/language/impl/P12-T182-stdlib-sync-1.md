# P12-T182 stdlib: sync（Mutex / RWMutex / WaitGroup）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `sync` 模块的基础同步原语（对齐 `stdlib/sync.md`）：互斥锁、读写锁、等待组，适配 mslang 协程调度器（协程友好，不阻塞 OS 线程）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T106 | 协程调度器 |
| P9-T108 | channel（内部实现可基于 channel） |

---

## API 清单

```ms
// Mutex（互斥锁）
mu := sync.Mutex()
mu.lock()         // 获取锁（协程让出而非 OS 阻塞）
mu.unlock()       // 释放锁
mu.tryLock() → bool  // 非阻塞尝试获取
// 上下文管理器
with mu:
    // 临界区
    pass

// RWMutex（读写锁）
rw := sync.RWMutex()
rw.rLock()      // 获取读锁（多协程可同时持有）
rw.rUnlock()    // 释放读锁
rw.lock()       // 获取写锁（排他）
rw.unlock()     // 释放写锁
rw.tryRLock() → bool
rw.tryLock() → bool
// 作为上下文管理器：默认写锁；rw.readLock() 返回读锁上下文管理器

// WaitGroup
wg := sync.WaitGroup()
wg.add(n)      // 增加计数（可多次调用）
wg.done()      // 减少 1（等同 add(-1)）
wg.wait()      // 阻塞直到计数归零（协程让出）

// Cond（条件变量）
cond := sync.Cond(mu)
cond.wait()    // 释放 mu 并等待，唤醒后重新获取 mu
cond.signal()  // 唤醒一个等待协程
cond.broadcast()  // 唤醒所有等待协程

// Semaphore
sem := sync.Semaphore(n)   // n = 初始计数
sem.acquire()
sem.release()
sem.tryAcquire() → bool
```

---

## 实现要点

```c
// 协程友好 Mutex：
// 内部：MsChannelObj（容量=1）作为令牌槽
// lock()：ch <- token（若 ch 满则协程 yield 等待空位）
// unlock()：<-ch（释放令牌）
// 等同：filled channel 作为互斥令牌

// 更高效实现（避免 channel GC 开销）：
// MsMutexObj：atomic flag(0=free, 1=locked) + 等待协程队列
// lock()：CAS(0→1) 成功则继续；失败则加入 waiters 队列 + yield
// unlock()：若 waiters 非空，唤醒第一个；否则 CAS(1→0)

typedef struct MsMutexObj {
  MsObject       header;
  _Atomic int    state;    // 0=free 1=locked
  MsCoroutineObj* waiters; // 链表（next 指针）
  MsMutex        spin;     // 保护 waiters 链表的自旋锁（短暂）
} MsMutexObj;

// RWMutex：
// state: readers_count + writer_waiting + writer_locked
// rLock()：若无写锁，原子增 readers
// lock()：先设 writer_waiting，等 readers==0，再获取写锁

// WaitGroup：
// 内部：atomic count + 条件变量（channel）
// add(n)：count += n
// done()：count -= 1；若 count == 0 → broadcast
// wait()：若 count > 0 → 加入 waiters + yield

// Cond 基于 waiters 队列 + Mutex
// wait()：加入 cond.waiters → mu.unlock() → yield → mu.lock()
// signal()：弹出 cond.waiters 首个 → 入调度器就绪队列
```

---

## 验收标准（checklist）

- [ ] 并发写计数器（1000 协程各加 1）+ Mutex → 结果精确 1000。
- [ ] RWMutex：多个读协程可并发，写协程排他等待。
- [ ] WaitGroup：主协程 wait() 直到所有子协程 done()。
- [ ] Cond.broadcast() 唤醒所有等待协程。
- [ ] `with mu:` 语法正常工作（异常时也正确 unlock）。
- [ ] Mutex 死锁检测（可选：超时 10s 后警告）。

---

## 测试用例（.ms）

```ms
import sync

// Mutex 并发计数
mu := sync.Mutex()
count := [0]
wg := sync.WaitGroup()
wg.add(1000)
for _ in range(1000) {
    go func() {
        with mu:
            count[0] = count[0] + 1
        wg.done()
    }()
}
wg.wait()
print(count[0])   // 1000（精确，无竞态）

// RWMutex 读多写少
rw := sync.RWMutex()
data := [0]
write_wg := sync.WaitGroup()
write_wg.add(10)
for _ in range(10) {
    go func() {
        with rw:   // 写锁
            data[0] = data[0] + 1
        write_wg.done()
    }()
}
for _ in range(100) {
    go func() {
        with rw.readLock():
            _ = data[0]   // 读操作
    }()
}
write_wg.wait()
print(data[0])   // 10

// Cond
mu2 := sync.Mutex()
cond := sync.Cond(mu2)
ready := [false]
go func() {
    import time; time.sleep(0.1)
    with mu2:
        ready[0] = true
        cond.signal()
}()
with mu2:
    while not ready[0]:
        cond.wait()
print(ready[0])  // true
```
