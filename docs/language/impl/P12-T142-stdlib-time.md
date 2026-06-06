# P12-T142 stdlib: time

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `time` 模块：时钟读取、睡眠、时间戳格式化、单调时钟、性能计时（perf_counter）、以及调度器集成的 `time.after`（与 channel/select 配合的定时器）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T106 | 调度器（time.after 需要定时触发协程） |
| P11-T130 | 扩展模块 API |

---

## API 清单

```ms
// 对齐 stdlib/time.md
time.now()              // → float  Unix 时间戳（秒，含小数）
time.now_ns()           // → int    纳秒精度 Unix 时间戳
time.monotonic()        // → float  单调时钟（秒，不可回拨）
time.perf_counter()     // → float  高精度计时器（最高精度）
time.process_time()     // → float  进程 CPU 时间
time.sleep(secs)        // 当前协程等待（不阻塞调度器）
time.strftime(fmt, t=nil) → str  格式化时间
time.strptime(s, fmt)   // → struct_time
time.gmtime(secs=nil)   // → struct_time（UTC）
time.localtime(secs=nil) // → struct_time（本地时间）
time.mktime(struct_time) // → float（struct_time → 时间戳）
time.timezone           // int  UTC 偏移秒数（本地时区）
time.tzname             // (str, str)  时区名称

// 协程友好睡眠
time.sleep(secs)        // 等待 secs 秒后唤醒当前协程（协作式）

// 定时器（与 channel/select 配合）
time.after(ms) → chan   // ms 毫秒后向 channel 发送当前时间戳
time.tick(ms)  → chan   // 每隔 ms 毫秒发送一次（周期性）
time.stopTimer(ch)      // 停止 tick/after 定时器

// 计时上下文
timer := time.Timer()
timer.start()
timer.stop() → float    // 返回经过时间（ms）
```

---

## 实现要点

```c
// time.now()：clock_gettime(CLOCK_REALTIME) / GetSystemTime
// time.monotonic()：CLOCK_MONOTONIC / QueryPerformanceCounter
// time.perf_counter()：最高精度，平台相关

// time.sleep() 在协程调度器中实现：
// 1. 将当前协程加入"定时器堆"（最小堆，按唤醒时间排序）
// 2. yield 到调度器
// 3. 调度器主循环检查定时器堆，到期则将协程入就绪队列

// time.after(ms) 实现：创建无缓冲 channel，在定时器到期时发送
// 值为当前时间戳（float）

typedef struct MsTimer {
    uint64_t       wakeNs;   // 唤醒时间（纳秒，单调时钟）
    MsCoroutineObj* coro;    // 或 channel
    MsChannelObj*  ch;       // time.after 的 channel
    bool           repeat;   // tick 模式
    uint64_t       intervalNs;
} MsTimer;

// 使用最小堆管理定时器（按 wakeNs 排序）
MsMinHeap gTimerHeap;

// 调度器主循环中：
// before picking next ready coro:
// check if any timers expired → move to ready queue
```

---

## 验收标准（checklist）

- [ ] `time.now()` 返回合理的 Unix 时间戳（> 1700000000）。
- [ ] `time.sleep(0.1)` 等待约 100ms 后协程恢复。
- [ ] `time.monotonic()` 单调不减（两次调用第二次 >= 第一次）。
- [ ] `time.after(100)` 创建 channel，100ms 后可接收值。
- [ ] `time.strftime("%Y-%m-%d")` 返回当前日期字符串。
- [ ] 1000 个 `time.sleep(0.01)` 协程并发不阻塞 OS 线程。

---

## 测试用例（.ms）

```ms
import time

t0 := time.now()
print(t0 > 1.7e9)   // true（> 2023年）

// 协程睡眠
go func() {
    time.sleep(0.1)
    print("woke up!")
}()

// 定时器
timeout := time.after(200)
select {
    case <-timeout: print("200ms passed")
}

// 性能计时
t := time.perf_counter()
s := 0
for i in range(1_000_000) { s = s + i }
dt := time.perf_counter() - t
print("1M loop:", dt * 1000, "ms")

// strftime
print(time.strftime("%Y-%m-%d %H:%M:%S"))  // 如 2024-01-15 10:30:00
```

---

## Benchmark

```ms
// 1000 个协程并发睡眠
import time
n := 1000
ch := make(chan, n)
t0 := time.now()
for i in range(n) {
    go func() {
        time.sleep(0.01)   // 10ms
        ch <- 1
    }()
}
for i in range(n) { <-ch }
t1 := time.now()
print("1000 concurrent sleep 10ms:", t1-t0, "s")
// 目标：~10ms（并行，不是 10s 串行）
```
