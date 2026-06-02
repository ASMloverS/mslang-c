# time — 时钟读取、睡眠与高精度计时

```ms
import time
```

## 概述

提供对操作系统时钟的访问、goroutine 睡眠和高精度性能计时。**不提供日期/时间
对象**（见 `datetime` 模块）。与 Go 调度器集成：`time.sleep` 挂起当前
goroutine 而不阻塞底层 OS 线程。

## 常量与类型

**`time.struct_time`** — 表示分解后的日历时间，字段均为只读整数：

| 字段 | 含义 | 范围 |
|---|---|---|
| `tm_year` | 年份 | 如 2026 |
| `tm_mon` | 月份 | 1–12 |
| `tm_mday` | 日 | 1–31 |
| `tm_hour` | 时 | 0–23 |
| `tm_min` | 分 | 0–59 |
| `tm_sec` | 秒 | 0–61（含闰秒） |
| `tm_wday` | 星期（0=周一…6=周日） | 0–6 |
| `tm_yday` | 年内天数 | 1–366 |
| `tm_isdst` | 夏令时标志 | −1/0/1 |

**时区常量：**

| 名称 | 说明 |
|---|---|
| `time.timezone` | 本地非夏令时与 UTC 的偏移秒数（西正东负） |
| `time.altzone` | 本地夏令时与 UTC 的偏移秒数 |
| `time.daylight` | 若本地定义了夏令时则非零 |
| `time.tzname` | 两个时区名称字符串的元组 `(非DST名, DST名)` |

## 函数签名速查

**时钟读取**

| 函数 | 签名 | 说明 |
|---|---|---|
| `time` | `time.time() → float` | 当前 Unix 时间戳（秒，含亚秒精度） |
| `time_ns` | `time.time_ns() → int` | 当前 Unix 时间戳（纳秒，int64） |
| `monotonic` | `time.monotonic() → float` | 单调时钟（秒）；只增不减，适合计时间隔 |
| `monotonic_ns` | `time.monotonic_ns() → int` | 单调时钟（纳秒） |
| `perf_counter` | `time.perf_counter() → float` | 最高精度时钟（秒）；最适合基准测试 |
| `perf_counter_ns` | `time.perf_counter_ns() → int` | 最高精度时钟（纳秒） |
| `process_time` | `time.process_time() → float` | 当前进程已使用的 CPU 时间（秒），非挂钟时间 |

**睡眠**

| 函数 | 签名 | 说明 |
|---|---|---|
| `sleep` | `time.sleep(seconds)` | 挂起当前 goroutine；`seconds` 可为 float，不阻塞 OS 线程 |

**时间转换**

| 函数 | 签名 | 说明 |
|---|---|---|
| `gmtime` | `time.gmtime(secs=nil) → struct_time` | Unix 时间戳 → UTC struct_time；nil 表示当前时间 |
| `localtime` | `time.localtime(secs=nil) → struct_time` | Unix 时间戳 → 本地 struct_time；nil 表示当前时间 |
| `mktime` | `time.mktime(t) → float` | 本地 struct_time → Unix 时间戳 |

**格式化与解析**

| 函数 | 签名 | 说明 |
|---|---|---|
| `strftime` | `time.strftime(format, t=nil) → str` | 按 Python % 代码格式化 struct_time；nil 使用本地当前时间 |
| `strptime` | `time.strptime(string, format) → struct_time` | 将字符串解析为 struct_time |
| `asctime` | `time.asctime(t=nil) → str` | 返回人类可读的时间字符串，如 `"Sun Jun  3 12:00:00 2026"` |
| `ctime` | `time.ctime(secs=nil) → str` | 同 asctime，但接受 Unix 时间戳；nil 表示当前时间 |

**调度器集成**

| 函数 | 签名 | 说明 |
|---|---|---|
| `after` | `time.after(seconds) → chan` | 返回一个 channel；`seconds` 秒后向该 channel 发送当前 Unix 时间戳（float）；适合在 `select` 中实现超时 |

## 详细语义

### time.time() 与 time.time_ns()

`time.time()` 返回自 Unix 纪元（1970-01-01 00:00:00 UTC）起经过的秒数，
为 float64，包含亚秒精度。`time.time_ns()` 返回相同原点的纳秒数（int64），
避免 float 精度损失。

系统时钟可能因 NTP 同步向前或向后跳跃；**不适合**测量时间间隔。

### time.monotonic() 与 time.perf_counter()

`monotonic()` 保证单调递增，适合计算两点之间的间隔（如超时检测）。
`perf_counter()` 使用系统提供的最高分辨率计时器，适合微基准测试。
两者的绝对值无意义，只有两次调用的**差值**才有意义。

### time.sleep(seconds)

`seconds` 可为浮点数，精度取决于操作系统（通常毫秒级）。挂起当前 goroutine
期间，调度器可以运行其他 goroutine，OS 线程不会阻塞。`seconds <= 0` 时
立即返回（等效于 `runtime.Gosched()`）。

### time.strftime(format, t=nil)

格式代码与 `datetime.strftime` 相同（`%Y`、`%m`、`%d`、`%H`、`%M`、`%S` 等）。
`t` 为 nil 时使用 `time.localtime()` 返回的当前本地时间。

### time.after(seconds) — 调度器集成

```ms
ch := time.after(5.0)
select {
case result := <-ch:
    fmt.println("触发时间戳:", result)
case data := <-otherChan:
    fmt.println("先收到数据:", data)
}
```

与 `time.sleep` 的区别：`after` 不阻塞当前 goroutine，返回的 channel
可与其他 channel 组合在 `select` 中使用，实现非阻塞超时控制。

## 示例

```ms
import time
import fmt

// 1. 用 perf_counter 测量耗时
func benchmark(label, f) {
    start := time.perf_counter()
    f()
    elapsed := time.perf_counter() - start
    fmt.printf("%s 耗时: %.6f 秒\n", label, elapsed)
}

// 2. sleep 不阻塞调度器
func delayed_hello() {
    time.sleep(0.5)
    fmt.println("0.5 秒后打印")
}

go delayed_hello()
fmt.println("主 goroutine 继续运行")

// 3. 格式化当前时间
now_str := time.strftime("%Y-%m-%d %H:%M:%S")
fmt.println("当前时间:", now_str)     // 如 "2026-06-03 15:04:05"

// 4. 解析时间字符串再格式化
t := time.strptime("2026-01-15 09:30:00", "%Y-%m-%d %H:%M:%S")
fmt.println(t.tm_year, t.tm_mon, t.tm_mday)  // 2026 1 15

// 5. 单调时钟计时
t0 := time.monotonic()
time.sleep(0.1)
fmt.printf("实际间隔: %.3f 秒\n", time.monotonic()-t0)

// 6. select 超时
ch := make(chan string)
go func() {
    time.sleep(2.0)
    ch <- "done"
}()

select {
case msg := <-ch:
    fmt.println("收到:", msg)
case <-time.after(1.0):
    fmt.println("超时")
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `strptime` 字符串与格式不匹配；`mktime` 接收到无效 struct_time |
| `OverflowError` | `time_ns`/`monotonic_ns`/`perf_counter_ns` 结果超出 int64 范围 |
| `OSError` | 底层系统调用失败（极少见） |
