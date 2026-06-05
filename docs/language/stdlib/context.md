# context — 取消/超时上下文传播

```ms
import context
```

## 概述

携带截止时间、取消信号和键值对数据跨 goroutine/async 边界传播。参考 Go
`context` 包设计。

Context 解决了一个核心问题：当一个请求（如 HTTP 请求）被取消或超时时，如何
通知所有为该请求工作的下游 goroutine 停止工作、释放资源。传递 Context 比通过
全局变量或额外的 `done` channel 更整洁，且支持层级取消——取消父 context 会
同时取消所有子 context。

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `context.Canceled` | `Exception` | context 被显式取消时的异常/哨兵值 |
| `context.DeadlineExceeded` | `Exception` | context 截止时间已过时的异常/哨兵值 |

**Context 接口**（所有 context 对象均实现）

| 方法 | 签名 | 说明 |
|---|---|---|
| `done` | `ctx.done() → chan` | 取消时关闭的 channel；用于 `select` |
| `err` | `ctx.err() → Exception \| nil` | 未取消时为 `nil`；已取消时为 `context.Canceled` 或 `context.DeadlineExceeded` |
| `deadline` | `ctx.deadline() → (float, bool)` | 截止时间（Unix 时间戳）与是否设置了截止时间 |
| `value` | `ctx.value(key) → any` | 查找关联的值；未找到返回 `nil` |

## 函数签名速查

**根 context**

| 函数 | 签名 | 说明 |
|---|---|---|
| `background` | `context.background() → Context` | 永不取消的根 context；用于顶层操作 |
| `todo` | `context.todo() → Context` | 占位根 context；表示"此处需要 context 但尚未确定" |

**派生 context**

| 函数 | 签名 | 说明 |
|---|---|---|
| `withCancel` | `context.withCancel(parent) → (Context, cancelFn)` | 返回可取消的子 context 和 `cancel()` 函数 |
| `withTimeout` | `context.withTimeout(parent, seconds) → (Context, cancelFn)` | `seconds` 秒后自动取消；也可手动调用 `cancel()` |
| `withDeadline` | `context.withDeadline(parent, deadline) → (Context, cancelFn)` | 在绝对时间（Unix 时间戳 float）取消 |
| `withValue` | `context.withValue(parent, key, value) → Context` | 附加键值对；不返回 cancelFn |

## 详细语义

### 根 context

`context.background()` 是所有 context 树的根，永远不会被取消、没有截止时间、
不携带任何值。在 main 函数、服务初始化或测试中作为顶层 context 使用。

`context.todo()` 语义上等同于 `background()`，但传达"此处的 context 需求
尚未明确"的意图，便于代码搜索和审查。**不要将 `nil` 传入期望 Context 的
函数**；当真正不需要 context 时使用 `background()`。

### context.withCancel

```ms
ctx, cancel := context.withCancel(parent)
```

返回 `parent` 的子 context。调用 `cancel()` 时：

1. `ctx.done()` 返回的 channel 被关闭。
2. `ctx.err()` 返回 `context.Canceled`。
3. 以 `ctx` 为 parent 派生的所有子 context 同时被取消（级联取消）。

**必须调用 `cancel()`**：即使操作正常完成，也必须调用 `cancel()` 以释放与
该 context 关联的内部资源，防止 goroutine 泄漏。使用 `try/finally`：

```ms
ctx, cancel := context.withCancel(parent)
try {
    doWork(ctx)
} finally {
    cancel()
}
```

### context.withTimeout

```ms
ctx, cancel := context.withTimeout(parent, 5.0)  // 5 秒超时
```

等价于 `withDeadline(parent, time.time() + seconds)`。超时触发时：

- `ctx.done()` channel 关闭。
- `ctx.err()` 返回 `context.DeadlineExceeded`。

返回的 `cancelFn` 仍应在 `finally` 中调用——若操作在超时前完成，提前调用
`cancel()` 可立即释放计时资源。

### context.withDeadline

```ms
import time
deadline := time.time() + 10.0  // 10 秒后
ctx, cancel := context.withDeadline(parent, deadline)
```

若 `parent` 已有更早的截止时间，则子 context 继承 parent 的截止时间（取较早
的那个）。`ctx.deadline()` 返回实际生效的截止时间。

### context.withValue

```ms
// 定义私有键类型以避免碰撞
class RequestIDKey {}
key := RequestIDKey()

ctx = context.withValue(parent, key, "req-42")
// 在下游取回
rid := ctx.value(key)  // "req-42"
```

值沿 context 树向下查找：`ctx.value(key)` 先查自身，未找到则递归查父 context。

**键的选择**：应使用自定义类型的实例作为键，而非字符串字面量，以避免不同
包之间的键名碰撞。

**用途限制**：`withValue` 应仅用于请求范围的元数据（请求 ID、认证令牌、
trace span 等），**不应**用于传递可选函数参数。

### ctx.done() 与 select

`ctx.done()` 返回的 channel 在 context 取消或超时时关闭（而非发送值）。关闭
的 channel 在 `select` 中总是就绪：

```ms
select {
case result := <-workChan:
    return result
case <-ctx.done():
    return nil, ctx.err()
}
```

未设置截止时间且永不取消的 context（如 `background()`），其 `done()` 返回
`nil`；在 `select` 中 `nil` channel 永远不就绪，因此该 case 永远不会被选中。

### 级联取消

取消总是从父向子传播：

```
background
  └── withTimeout(30s) ← 请求根
        ├── withCancel ← 子操作 A
        └── withCancel ← 子操作 B
```

取消请求根 context 后，子操作 A 和 B 的 context 同时被取消。子操作调用
自己的 `cancel()` 不会影响父或兄弟 context。

## 示例

### HTTP 请求超时

```ms
import context
import http
import fmt

async func fetch(url) {
    ctx, cancel := context.withTimeout(context.background(), 10.0)
    try {
        resp := await http.get(url, ctx=ctx)
        return await resp.text()
    } finally {
        cancel()
    }
}
```

### fan-out 共享取消

```ms
import context
import sync
import fmt

async func doSearch(ctx, query, source) {
    // 检查 context 是否已取消
    if ctx.err() != nil {
        return nil
    }
    // 模拟搜索
    return await searchBackend(ctx, query, source)
}

async func parallelSearch(query) {
    ctx, cancel := context.withTimeout(context.background(), 5.0)
    try {
        resultsCh := make(chan, 3)
        sources := ["db", "cache", "index"]
        // go 启动的 goroutine 与 async func 共享统一调度器，
        // 因此 goroutine 闭包内可直接使用 await
        for src in sources {
            go func(s) {
                r := await doSearch(ctx, query, s)
                resultsCh <- r
            }(src)
        }

        results := []
        for i := 0; i < len(sources); i++ {
            r := <-resultsCh
            if r != nil {
                results.append(r)
            }
        }
        return results
    } finally {
        cancel()  // 任一分支完成后取消其余
    }
}
```

### 在循环中检查取消

```ms
import context

func processItems(ctx, items) {
    for item in items {
        // 在每次迭代开始时检查取消信号
        if ctx.err() != nil {
            raise ctx.err()
        }
        heavyProcess(item)
    }
}
```

### select 监听取消

```ms
import context

func streamData(ctx, ch) {
    for {
        select {
        case data := <-ch:
            handle(data)
        case <-ctx.done():
            fmt.println("已取消:", ctx.err())
            return
        }
    }
}
```

### 通过 context 传递请求元数据

```ms
import context
import fmt

// 定义私有键类型
class TraceIdKey {}
TRACE_KEY := TraceIdKey()

func withTraceId(ctx, tid) {
    return context.withValue(ctx, TRACE_KEY, tid)
}

func getTraceId(ctx) {
    return ctx.value(TRACE_KEY)
}

func handleRequest(req) {
    ctx := withTraceId(context.background(), req.traceId)
    ctx, cancel := context.withTimeout(ctx, 30.0)
    try {
        process(ctx, req)
    } finally {
        cancel()
    }
}

func process(ctx, req) {
    tid := getTraceId(ctx)
    fmt.printf("[%s] 处理请求\n", tid)
    // tid 在整个调用链中自动传递
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `context.Canceled` | context 被显式取消（`cancel()` 被调用） |
| `context.DeadlineExceeded` | context 的截止时间已过 |
| `ValueError` | `withTimeout` 的 `seconds` 为负数 |
| `TypeError` | `withDeadline` 的 `deadline` 不是数值类型 |
