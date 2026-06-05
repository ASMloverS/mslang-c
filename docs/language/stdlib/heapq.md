# heapq — 堆队列（最小堆优先队列）

```ms
import heapq
```

## 概述

基于列表的最小堆操作。堆不变量：`heap[0]` 始终为最小元素，且任意节点的值
不大于其子节点的值。所有函数直接作用于普通 `list`，不引入独立的堆类型。

堆元素必须支持 `<` 比较。若需自定义排序顺序，将元素包装为 `(priority, item)`
元组——元组按字段逐一比较，`priority` 决定顺序，`item` 作为次级比较键。

## 常量与类型

本模块不定义常量或类型，仅提供操作普通 `list` 的函数。

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `heapPush` | `heapPush(heap, item)` | 压入元素并维护堆不变量 |
| `heapPop` | `heapPop(heap) → item` | 弹出并返回最小元素 |
| `heapPushPop` | `heapPushPop(heap, item) → item` | 先压入再弹出最小值（比分开调用更高效） |
| `heapReplace` | `heapReplace(heap, item) → item` | 先弹出最小值再压入新元素（比分开调用更高效） |
| `heapify` | `heapify(lst)` | 原地将列表转换为堆，O(n) |
| `nLargest` | `nLargest(n, iter, key=nil) → list` | 返回最大的 n 个元素 |
| `nSmallest` | `nSmallest(n, iter, key=nil) → list` | 返回最小的 n 个元素 |
| `merge` | `merge(*iters, key=nil, reverse=false) → iterator` | 惰性合并多个已排序可迭代对象 |

## 详细语义

### heapPush

```
heapq.heapPush(heap, item)
```

将 `item` 压入堆，并在 O(log n) 时间内恢复堆不变量。`heap` 必须在调用前已是合法堆
（可由 `heapify` 初始化，或从空列表开始逐步 `heapPush`）。

### heapPop

```
heapq.heapPop(heap) → item
```

移除并返回堆中最小的元素（即 `heap[0]`），O(log n)。堆为空时抛 `IndexError`。

### heapPushPop

```
heapq.heapPushPop(heap, item) → item
```

等效于先 `heapPush(heap, item)` 再 `heapPop(heap)`，但仅一次堆调整，更高效。
返回的最小值可能是刚压入的 `item`（若 `item` ≤ 当前堆顶）。堆可以为空。

### heapReplace

```
heapq.heapReplace(heap, item) → item
```

等效于先 `heapPop(heap)` 再 `heapPush(heap, item)`，但仅一次堆调整。
返回被弹出的旧最小值。堆为空时抛 `IndexError`。

与 `heapPushPop` 的区别：`heapReplace` 总是先弹出已有最小值再压入，
因此返回值必定 ≤ `item`；而 `heapPushPop` 在 `item` 更小时直接返回 `item`。

### heapify

```
heapq.heapify(lst)
```

将任意列表**原地**转换为满足堆不变量的列表，O(n)。比逐个 `heapPush` 更高效。

### nLargest / nSmallest

```
heapq.nLargest(n, iter, key=nil) → list
heapq.nSmallest(n, iter, key=nil) → list
```

从可迭代对象中返回最大/最小的 `n` 个元素，结果已排序（`nLargest` 降序，
`nSmallest` 升序）。

`key` 为单参数函数，用于从元素中提取比较键，不影响返回值内容。

当 `n` 远小于序列长度时，此函数比先排序再切片更高效（O(m log n)，m 为序列长度）。
当 `n` 接近序列长度时，直接排序可能更快。

### merge

```
heapq.merge(*iters, key=nil, reverse=false) → iterator
```

合并多个**已排序**的可迭代对象，返回惰性迭代器，产出元素保持全局排序顺序。
不将所有元素加载到内存中。

- `key`：提取比较键的函数，各输入序列须按同一 key 排序。
- `reverse=true`：各输入序列须为降序，输出也为降序。

## 示例

```ms
import heapq

// 任务优先级队列（priority 越小越优先）
tasks := []
heapq.heapPush(tasks, (3, "低优先级任务"))
heapq.heapPush(tasks, (1, "紧急任务"))
heapq.heapPush(tasks, (2, "普通任务"))

for len(tasks) > 0 {
    priority, name := heapq.heapPop(tasks)
    fmt.println($"[{priority}] {name}")
}
// [1] 紧急任务
// [2] 普通任务
// [3] 低优先级任务

// 原地堆化已有列表
data := [5, 1, 8, 2, 9, 3]
heapq.heapify(data)
fmt.println(data[0])  // 1（最小值）

// 从大量数据中取前 3 名
scores := [42, 17, 95, 63, 88, 51, 74]
fmt.println(heapq.nLargest(3, scores))   // [95, 88, 74]
fmt.println(heapq.nSmallest(3, scores))  // [17, 42, 51]

// 按 key 提取
records := [{"name": "alice", "score": 80}, {"name": "bob", "score": 95}]
top := heapq.nLargest(1, records, key=func(r) { return r["score"] })
fmt.println(top[0]["name"])  // bob

// 合并已排序序列
a := [1, 3, 5]
b := [2, 4, 6]
fmt.println(list(heapq.merge(a, b)))  // [1, 2, 3, 4, 5, 6]
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `IndexError` | `heapPop` 或 `heapReplace` 在空堆上调用 |
| `TypeError` | 元素不支持 `<` 比较（类型不可比较） |
