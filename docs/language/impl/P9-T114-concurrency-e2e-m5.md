# P9-T114 并发 .ms 测试套件（M5 里程碑）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

通过完整的 `.ms` 测试套件验证 P9 并发系统（T106–T113）：协程创建、channel 通信、select 路由、async/await、M:N 调度、安全点抢占。此任务是 P9 的**里程碑收口**（M5）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T106 ~ T113 | P9 所有任务 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `concurrency.md` | §1 概述 / §2 协程 / §3 Channel / §4 select / §5 调度器 |

---

## M5 测试套件（`tests/ms/p9/`）

### `tests/ms/p9/producer_consumer.ms`

```ms
// 生产者-消费者（有缓冲 channel）
ch := make(chan, 5)
results := []

go func() {
    for i in range(10) {
        ch <- i
    }
    close(ch)
}()

go func() {
    for v in ch {
        results.append(v)
    }
}()

// 期望：results = [0,1,2,3,4,5,6,7,8,9]（顺序保证）
print(results)
```

**期望输出**：
```
[0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
```

### `tests/ms/p9/fan_out_fan_in.ms`

```ms
// Fan-out / Fan-in：将工作分发到多个 worker，收集结果
func worker(in, out, id) {
    for v in in {
        out <- (v * v)  // 平方计算
    }
}

jobs := make(chan, 10)
results := make(chan, 10)

// 启动 3 个 worker
for i in range(3) {
    go worker(jobs, results, i)
}

// 发送 9 个任务
for i in range(1, 10) { jobs <- i }
close(jobs)

// 收集结果（顺序不确定）
squares := sorted([<-results for _ in range(9)])
print(squares)  // [1, 4, 9, 16, 25, 36, 49, 64, 81]
```

### `tests/ms/p9/select_timeout.ms`

```ms
import time

func withTimeout(ch, ms) {
    timeout := time.after(ms)
    select {
        case v := <-ch:   return v, false   // (value, timedOut)
        case <-timeout:   return nil, true
    }
}

ch := make(chan)
v, to := withTimeout(ch, 50)
print(to)   // true（50ms 内无发送）

go func() { ch <- 42 }()
v, to = withTimeout(ch, 100)
print(v, to)  // 42 false
```

**期望输出**：
```
true
42 false
```

### `tests/ms/p9/async_await_chain.ms`

```ms
async func step1(x) { return x + 1 }
async func step2(x) { return x * 2 }
async func step3(x) { return x - 3 }

async func pipeline(n) {
    a := await step1(n)
    b := await step2(a)
    c := await step3(b)
    return c
}

async func main() {
    result := await pipeline(10)
    print(result)   // (10+1)*2-3 = 19
}
go main()
```

**期望输出**：
```
19
```

### `tests/ms/p9/goroutine_isolation.ms`

```ms
// 协程内异常不影响主程序
go func() {
    raise ValueError("goroutine error")
}()

go func() {
    print("I survived!")
}()
// stderr: [goroutine] ValueError: goroutine error
// stdout: I survived!
```

---

## 验收标准（checklist）

- [ ] `tests/ms/p9/producer_consumer.ms` 通过。
- [ ] `tests/ms/p9/fan_out_fan_in.ms` 通过（多 worker 并发）。
- [ ] `tests/ms/p9/select_timeout.ms` 通过。
- [ ] `tests/ms/p9/async_await_chain.ms` 通过。
- [ ] `tests/ms/p9/goroutine_isolation.ms`：协程异常不杀死主程序。
- [ ] 死锁检测：所有协程等待时，打印 "deadlock!" 并退出 1。
- [ ] M:N（`MSLANG_WORKERS=4`）下所有测试通过。
- [ ] 安全点抢占：CPU 密集型协程在 20ms 内被抢占。

---

## Benchmark（M5）

```ms
// benchmarks/bench_concurrency.ms

import time

// 1. channel 吞吐
ch := make(chan, 1000)
n := 1_000_000
t0 := time.now()
go func() { for i in range(n) { ch <- i } ; close(ch) }()
sum := 0
go func() { for v in ch { sum = sum + v } }()
t1 := time.now()
print("1M channel ops:", t1-t0, "ms")
// 目标（单线程）: < 500ms；目标（4 Worker）: < 200ms

// 2. async/await 链延迟
async func noop(x) { return x }
async func bench_async() {
    s := 0
    for i in range(10_000) {
        s = s + (await noop(i))
    }
    print("async chain 10K:", s)
}
t2 := time.now()
go bench_async()
t3 := time.now()
print("async bench:", t3-t2, "ms")
// 目标 < 100ms
```

---

## 风险与边界

- **`time.after`**：T114 测试依赖 `time.after(ms)`，但 `time` 模块在 P12 实现；M5 前需要提供一个最小版本 `time.after`（基于调度器定时任务）。
- **M5 定义**：M5 通过 = 协程 + channel + select + async/await + 基础 M:N 调度全部可工作；部分高级特性（抢占精度、GC 并发）在 P10 完善。
