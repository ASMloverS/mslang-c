# sync — 同步原语（互斥锁、等待组、原子操作）

```ms
import sync
```

## 概述

为 goroutine 提供基本同步原语：互斥锁、读写锁、等待组、单次初始化、条件变量、
并发安全 map 以及原子操作。参考 Go `sync` 包设计。

**优先使用 channel**：当并发协调可以用 `go`+channel 表达时，应优先选择 channel；
channel 的通信模型更清晰、更不易出错。当需要保护**共享内存**的并发访问（低延迟
临界区、缓存共享状态等）时，再使用 `sync`。

mslang **没有 GIL**，多个 goroutine 真正并行运行于多个 OS 线程，未经保护的共享
可变状态是未定义行为。

## 常量与类型

| 名称 | 说明 |
|---|---|
| `sync.Mutex` | 互斥锁构造函数 |
| `sync.RWMutex` | 读写锁构造函数 |
| `sync.WaitGroup` | 等待组构造函数 |
| `sync.Once` | 单次初始化构造函数 |
| `sync.Cond` | 条件变量构造函数 |
| `sync.Map` | 并发安全 map 构造函数 |
| `sync.atomic` | 原子操作子模块 |

## 函数签名速查

**sync.Mutex**

| 方法 | 签名 | 说明 |
|---|---|---|
| `lock` | `mu.lock()` | 获取锁；已被持有则阻塞当前 goroutine |
| `unlock` | `mu.unlock()` | 释放锁；未持有则 panic |
| `tryLock` | `mu.tryLock() → bool` | 非阻塞尝试获取；成功返回 `true` |

**sync.RWMutex**

| 方法 | 签名 | 说明 |
|---|---|---|
| `lock` | `rw.lock()` | 获取独占写锁 |
| `unlock` | `rw.unlock()` | 释放写锁 |
| `tryLock` | `rw.tryLock() → bool` | 非阻塞尝试获取写锁 |
| `rLock` | `rw.rLock()` | 获取共享读锁（多个读者可同时持有） |
| `rUnlock` | `rw.rUnlock()` | 释放读锁 |
| `tryRLock` | `rw.tryRLock() → bool` | 非阻塞尝试获取读锁 |

**sync.WaitGroup**

| 方法 | 签名 | 说明 |
|---|---|---|
| `add` | `wg.add(n)` | 计数器加 `n`（`n` 可为负数） |
| `done` | `wg.done()` | 计数器减 1，等价于 `add(-1)` |
| `wait` | `wg.wait()` | 阻塞直到计数器归零 |

**sync.Once**

| 方法 | 签名 | 说明 |
|---|---|---|
| `do` | `once.do(fn)` | 保证 `fn` 只执行一次，即使并发调用 |

**sync.Cond**

| 方法 | 签名 | 说明 |
|---|---|---|
| `wait` | `cond.wait()` | 原子释放关联锁并挂起；被唤醒后重新获取锁 |
| `signal` | `cond.signal()` | 唤醒一个在 `wait()` 上阻塞的 goroutine |
| `broadcast` | `cond.broadcast()` | 唤醒所有在 `wait()` 上阻塞的 goroutine |

**sync.Map**

| 方法 | 签名 | 说明 |
|---|---|---|
| `store` | `m.store(key, value)` | 存储键值对 |
| `load` | `m.load(key) → (value, ok)` | 读取键；`ok=false` 表示不存在 |
| `delete` | `m.delete(key)` | 删除键 |
| `loadOrStore` | `m.loadOrStore(key, value) → (actual, loaded)` | 若键存在则返回已有值；否则存入并返回 `value` |
| `range` | `m.range(fn)` | 遍历所有键值对；`fn(key, value)` 返回 `false` 时停止 |

**sync.atomic**

| 函数 | 签名 | 说明 |
|---|---|---|
| `newRef` | `sync.atomic.newRef(initial) → ref` | 创建原子引用单元 |
| `load` | `sync.atomic.load(ref) → value` | 原子读取 |
| `store` | `sync.atomic.store(ref, val)` | 原子写入 |
| `add` | `sync.atomic.add(ref, delta) → oldValue` | 原子加法，返回旧值 |
| `compareSwap` | `sync.atomic.compareSwap(ref, old, new) → bool` | CAS：当前值等于 `old` 时替换为 `new` |
| `swap` | `sync.atomic.swap(ref, new) → oldValue` | 原子交换，返回旧值 |

## 详细语义

### sync.Mutex

`sync.Mutex()` 创建一个互斥锁，初始未锁定。

```ms
mu := sync.Mutex()
```

- `lock()` 若锁已被其他 goroutine 持有，则阻塞当前 goroutine（`CORO_WAITING`）
  直到锁可用。同一 goroutine 重复 `lock()` 会死锁（非可重入锁）。
- `unlock()` 若调用方未持有该锁则 panic。
- `tryLock()` 非阻塞：若锁空闲则获取并返回 `true`；否则立即返回 `false`，
  当前 goroutine 不挂起。

**最佳实践**：始终用 `defer` 或 `try/finally` 配对 `lock/unlock`，防止
因异常或提前返回导致死锁：

```ms
mu.lock()
try {
    // 临界区
} finally {
    mu.unlock()
}
```

### sync.RWMutex

读写锁允许多个 goroutine 同时持有**读锁**，但写锁是独占的：持有写锁时无法
获取读锁，反之亦然。

```ms
rw := sync.RWMutex()
```

- 读路径：`rLock()` / `rUnlock()`；多个读者可并发。
- 写路径：`lock()` / `unlock()`；独占，排斥所有读者和写者。
- **不可升级**：在持有读锁的情况下调用 `lock()` 会死锁，因为写锁需等待所有
  读锁释放，而当前 goroutine 本身持有一个未释放的读锁。

### sync.WaitGroup

用于等待一组 goroutine 全部完成。

```ms
wg := sync.WaitGroup()
```

- `add(n)` **必须在启动 goroutine 之前调用**；在 goroutine 内部调用 `add`
  存在竞态风险（`wait` 可能在 `add` 之前看到计数器为零而提前返回）。
- `done()` 等价于 `add(-1)`；通常在 goroutine 函数中通过 `defer` 调用。
- `done()` 调用次数超过 `add()` 累计增量时 panic（计数器不允许为负）。
- `wait()` 在计数器已为零时立即返回。

### sync.Once

保证某个函数在整个程序生命周期中只执行一次，即使多个 goroutine 同时调用
`do(fn)` 也是如此。

```ms
once := sync.Once()
```

- `fn` 必须无参数、无返回值。
- 若 `fn` 内部发生 panic，`Once` 仍视为"已完成"——后续的 `do(fn)` 调用
  **不会重试** `fn`，panic 会照常向上传播。

### sync.Cond

条件变量，用于 goroutine 之间基于条件的等待与通知。构造时传入一个已创建的
`Mutex`（或 `RWMutex`）作为关联锁：

```ms
mu := sync.Mutex()
cond := sync.Cond(mu)
```

- `wait()` 原子地释放关联锁并挂起当前 goroutine；被 `signal()` 或
  `broadcast()` 唤醒后，`wait()` 会重新获取关联锁再返回。
- **必须在持有关联锁时调用 `wait()`**，否则行为未定义。
- 由于虚假唤醒（spurious wakeup）的可能性，**始终在循环中检查条件**：

```ms
mu.lock()
try {
    for !condition {
        cond.wait()
    }
    // 安全地使用受保护的资源
} finally {
    mu.unlock()
}
```

- `signal()` 唤醒等待队列中的一个 goroutine（不保证顺序）。
- `broadcast()` 唤醒所有等待的 goroutine，适合条件变化影响多个等待者的场景。

### sync.Map

并发安全的 map，内部通过分离读写路径实现低竞争。

```ms
m := sync.Map()
```

- `load(key)` 返回 `(value, ok)` 元组；`ok=false` 表示键不存在。
- `loadOrStore(key, value)` 返回 `(actual, loaded)`：若键已存在，
  `actual` 为已有值，`loaded=true`；若键不存在，存入 `value`，返回
  `(value, false)`。
- `range(fn)` 遍历快照中的所有条目，`fn(key, value)` 返回 `false` 则停止。
  遍历期间的并发写入可能不会被 `range` 看到。

**何时选择 sync.Map vs map+Mutex**：`sync.Map` 适合键集合稳定、读多写少，
或每个键只写入一次后大量读取的场景。对于读写比例均衡或键集合频繁变化的
情况，普通 map+Mutex 通常更简单、性能更好。

### sync.atomic

原子操作在不使用锁的情况下对单个值进行线程安全的读写，适合计数器、状态标志
等简单共享变量。

**引用单元（ref）**：原子操作作用于通过 `newRef` 创建的不透明引用单元，而非
直接操作普通变量。这确保运行时可以正确对齐内存并追踪原子访问：

```ms
counter := sync.atomic.newRef(0)
```

- `load(ref)` — 原子读取当前值，保证可见性。
- `store(ref, val)` — 原子写入，其他 goroutine 的后续 `load` 一定能看到。
- `add(ref, delta)` — 原子加 `delta`，返回加之前的旧值；`delta` 可为负数。
- `compareSwap(ref, old, new)` — CAS：若当前值等于 `old` 则替换为 `new` 并
  返回 `true`；否则不修改并返回 `false`。
- `swap(ref, new)` — 原子设置为 `new`，返回旧值。

原子操作提供顺序一致性保证（sequentially consistent ordering）。对于需要
保护多个变量或多步操作的临界区，应使用 `Mutex` 而非原子操作。

## 示例

### fan-out 等待（WaitGroup）

```ms
import sync
import fmt

func worker(id, wg) {
    defer wg.done()
    fmt.printf("worker %d 开始\n", id)
    // 模拟工作
    fmt.printf("worker %d 完成\n", id)
}

func main() {
    wg := sync.WaitGroup()
    for i := 0; i < 5; i++ {
        wg.add(1)
        go worker(i, wg)
    }
    wg.wait()
    fmt.println("所有 worker 已完成")
}
```

### 共享计数器（Mutex）

```ms
import sync
import fmt

func makeCounter() {
    mu := sync.Mutex()
    count := 0

    func increment() {
        mu.lock()
        try {
            count += 1
        } finally {
            mu.unlock()
        }
    }

    func get() {
        mu.lock()
        try {
            return count
        } finally {
            mu.unlock()
        }
    }

    return increment, get
}

increment, get := makeCounter()
wg := sync.WaitGroup()
for i := 0; i < 100; i++ {
    wg.add(1)
    go func() {
        defer wg.done()
        increment()
    }()
}
wg.wait()
fmt.println("最终计数:", get())  // 100
```

### 惰性初始化（Once）

```ms
import sync

config := nil
once := sync.Once()

func getConfig() {
    once.do(func() {
        // 只执行一次，即使多个 goroutine 同时调用
        config = loadConfigFromDisk()
    })
    return config
}
```

### 生产者-消费者（Cond）

```ms
import sync
import fmt

func makeQueue(capacity) {
    mu := sync.Mutex()
    notEmpty := sync.Cond(mu)
    notFull := sync.Cond(mu)
    items := []

    func put(item) {
        mu.lock()
        try {
            for len(items) >= capacity {
                notFull.wait()
            }
            items.append(item)
            notEmpty.signal()
        } finally {
            mu.unlock()
        }
    }

    func get() {
        mu.lock()
        try {
            for len(items) == 0 {
                notEmpty.wait()
            }
            item := items[0]
            items = items[1:]
            notFull.signal()
            return item
        } finally {
            mu.unlock()
        }
    }

    return put, get
}
```

### 原子计数器（atomic）

```ms
import sync
import fmt

counter := sync.atomic.newRef(0)

wg := sync.WaitGroup()
for i := 0; i < 1000; i++ {
    wg.add(1)
    go func() {
        defer wg.done()
        sync.atomic.add(counter, 1)
    }()
}
wg.wait()
fmt.println("原子计数:", sync.atomic.load(counter))  // 1000
```

### RWMutex 缓存保护

```ms
import sync

cache := {}
rw := sync.RWMutex()

func readCache(key) {
    rw.rLock()
    try {
        return cache.get(key, nil)
    } finally {
        rw.rUnlock()
    }
}

func writeCache(key, value) {
    rw.lock()
    try {
        cache[key] = value
    } finally {
        rw.unlock()
    }
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `RuntimeError` | `unlock()` 在未持有锁时调用；`WaitGroup` 计数器降至负数 |
| `RuntimeError` | `Cond.wait()` 未在持有关联锁时调用 |
| `RuntimeError` | `Once.do()` 的 `fn` 接受参数（`fn` 必须无参） |
