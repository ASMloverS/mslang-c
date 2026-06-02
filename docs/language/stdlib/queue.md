# queue — 线程安全队列

```ms
import queue
```

## 概述

用于多 goroutine 之间安全传递数据的队列。提供 FIFO、LIFO 和优先级三种变体，
均支持阻塞式 `put`/`get` 和可选超时，以及任务完成追踪（`task_done`/`join`）。

与内置 `chan`（channel）的选择建议：

- 简单的单生产者/消费者场景 → 优先使用 `make(chan)`，语义更直接。
- 需要优先级排序 → 使用 `queue.PriorityQueue`。
- 需要追踪每个任务是否处理完毕（`join`）→ 使用 `queue.Queue`。
- 需要 Python 兼容 API → 使用本模块。

## 常量与类型

| 名称 | 说明 |
|---|---|
| `queue.Queue` | FIFO 队列类 |
| `queue.LifoQueue` | LIFO 队列类（栈） |
| `queue.PriorityQueue` | 最小优先队列类 |
| `queue.Empty` | 异常：队列为空时的 get 超时 |
| `queue.Full` | 异常：队列已满时的 put 超时 |

## 函数签名速查

以下方法三种队列类型（`Queue`、`LifoQueue`、`PriorityQueue`）均支持：

| 方法 | 签名 | 说明 |
|---|---|---|
| 构造 | `Queue(maxsize=0)` | maxsize=0 表示无限容量 |
| `put` | `put(item, block=true, timeout=nil)` | 入队；满时阻塞或超时 |
| `get` | `get(block=true, timeout=nil) → item` | 出队；空时阻塞或超时 |
| `put_nowait` | `put_nowait(item)` | 等同于 `put(item, block=false)` |
| `get_nowait` | `get_nowait() → item` | 等同于 `get(block=false)` |
| `task_done` | `task_done()` | 通知一个已出队的任务已处理完毕 |
| `join` | `join()` | 阻塞直到所有已入队任务均调用过 `task_done` |
| `qsize` | `qsize() → int` | 当前队列中的元素个数 |
| `empty` | `empty() → bool` | 队列是否为空 |
| `full` | `full() → bool` | 队列是否已满（maxsize=0 时永远返回 false） |

## 详细语义

### Queue

```
queue.Queue(maxsize=0)
```

FIFO 队列。`get` 返回最先入队的元素。`maxsize=0` 表示无限容量。

### LifoQueue

```
queue.LifoQueue(maxsize=0)
```

LIFO 队列（栈）。`get` 返回最后入队的元素。API 与 `Queue` 完全相同。

### PriorityQueue

```
queue.PriorityQueue(maxsize=0)
```

最小优先队列。`get` 返回优先级最低（值最小）的元素。元素须支持 `<` 比较。
推荐使用 `(priority, item)` 元组，以便在优先级相同时用 `item` 作为次级比较键。

### put

```
q.put(item, block=true, timeout=nil)
```

将 `item` 加入队列。

- `block=true, timeout=nil`：若队列已满，永久阻塞直到有空位。
- `block=true, timeout=n`：阻塞最多 `n` 秒；超时后抛 `queue.Full`。
- `block=false`：若队列已满，立即抛 `queue.Full`。

`maxsize=0` 时队列不会满，`put` 永不阻塞。

### get

```
q.get(block=true, timeout=nil) → item
```

从队列中取出并返回一个元素。

- `block=true, timeout=nil`：若队列为空，永久阻塞直到有元素。
- `block=true, timeout=n`：阻塞最多 `n` 秒；超时后抛 `queue.Empty`。
- `block=false`：若队列为空，立即抛 `queue.Empty`。

### task_done / join

```
q.task_done()
q.join()
```

用于追踪任务完成状态。工作模式：

1. 生产者调用 `put(item)` 入队，内部计数器递增。
2. 消费者调用 `get()` 取出任务，处理完毕后调用 `task_done()`，计数器递减。
3. 主线程调用 `join()` 阻塞，直到计数器归零。

`task_done` 调用次数超过 `get` 次数时抛 `ValueError`。

## 示例

```ms
import queue

// Worker pool：3 个消费者 goroutine 处理任务
q := queue.Queue()

// 生产者：向队列填入 10 个任务
for i in range(10) {
    q.put(i)
}

// 消费者 worker
func worker(id) {
    for {
        try {
            item := q.get_nowait()
            fmt.println($"worker {id} 处理任务 {item}")
            q.task_done()
        } catch (e: queue.Empty) {
            break  // 队列已空，退出循环
        }
    }
}

// 启动 3 个 goroutine
for i in range(3) {
    go worker(i)
}

q.join()  // 等待所有任务完成
fmt.println("所有任务已处理完毕")

// PriorityQueue 示例
pq := queue.PriorityQueue()
pq.put((3, "低优先级"))
pq.put((1, "紧急"))
pq.put((2, "普通"))

for !pq.empty() {
    priority, task := pq.get()
    fmt.println($"[{priority}] {task}")
}
// [1] 紧急
// [2] 普通
// [3] 低优先级
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `queue.Empty` | `get(block=false)` 或 `get_nowait()` 在空队列上调用；`get` 超时 |
| `queue.Full` | `put(block=false)` 或 `put_nowait()` 在满队列上调用；`put` 超时 |
| `ValueError` | `task_done` 调用次数超过已出队的元素数 |
| `TypeError` | `PriorityQueue` 中元素不支持 `<` 比较 |
