# P12-T147 stdlib: heapq / queue

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `heapq` 模块（最小堆算法，对齐 `stdlib/heapq.md`）和 `queue` 模块（线程安全队列，对齐 `stdlib/queue.md`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T059 | MsListObj（heapq 操作 list） |
| P9-T108 | channel/sync 原语（queue 用于并发） |

---

## API 清单

```ms
// heapq — 操作已有 list，维护最小堆不变式
heapq.heapify(list)               // 原地建堆，O(n)
heapq.heappush(heap, item)        // 推入元素，O(log n)
heapq.heappop(heap) → item        // 弹出最小值，O(log n)
heapq.heappushpop(heap, item) → item  // push 后 pop（比分别调用快）
heapq.heapreplace(heap, item) → item  // pop 后 push（原堆顶不变如更小则抛 Empty）
heapq.nsmallest(n, iterable, key=nil) → list  // 前 n 小
heapq.nlargest(n, iterable, key=nil)  → list  // 前 n 大
heapq.merge(*iterables, key=nil, reverse=false) → iterator  // 有序归并

// queue — 线程安全队列（协程间通信基础）
q := queue.Queue(maxsize=0)       // maxsize=0 → 无限制
q.put(item, block=true, timeout=nil)    // 入队（阻满时阻塞）
q.get(block=true, timeout=nil) → item  // 出队（空时阻塞）
q.put_nowait(item)                // 等同 put(block=false)
q.get_nowait() → item            // 等同 get(block=false)
q.task_done()                    // 通知 join() 一个任务已完成
q.join()                         // 阻塞直到所有任务 task_done
q.qsize() → int
q.empty() → bool
q.full() → bool

queue.LifoQueue(maxsize=0)       // 栈（LIFO）
queue.PriorityQueue(maxsize=0)   // 最小优先队列（元素需可比较）
queue.Empty                      // 异常：get_nowait 空时抛
queue.Full                       // 异常：put_nowait 满时抛
```

---

## 实现要点

```c
// heapq：对 MsListObj 直接操作（无需额外结构）
// sift_up / sift_down 用 msValueLt 比较（支持 key 函数时包装）

// heapq.merge：使用最小堆维护各迭代器当前值，O(n log k)

// queue.Queue：内部 deque + Mutex + 条件变量（或协程 channel）
// 协程安全：put/get 使用 MsChannelObj（P9-T108）作为底层
// task_done/join：unfinished_tasks 计数 + 条件变量

typedef struct MsQueueObj {
  MsObject     header;
  MsDequeObj*  deque;
  int64_t      maxsize;       // 0 = unlimited
  int64_t      unfinished;    // for join()
  MsChannelObj* notFull;     // 信号：有空位
  MsChannelObj* notEmpty;    // 信号：有元素
  MsMutex      mu;
} MsQueueObj;
```

---

## 验收标准（checklist）

- [ ] `heapq.heapify([3,1,4,1,5])` 后 `heap[0]` 为最小值。
- [ ] 连续 heappush/heappop 等价于排序（heap sort 正确性）。
- [ ] `heapq.nsmallest(3, [5,3,1,4,2])` → `[1,2,3]`。
- [ ] `queue.Queue()` 在协程间可安全 put/get。
- [ ] `queue.Queue(maxsize=1)` put 第二个时阻塞，直到 get。
- [ ] `PriorityQueue` 按优先级顺序 get。

---

## 测试用例（.ms）

```ms
import heapq

// 基础 heap sort
data := [3,1,4,1,5,9,2,6]
heapq.heapify(data)
result := []
while len(data) > 0 { result.append(heapq.heappop(data)) }
print(result)  // [1,1,2,3,4,5,6,9]

// nlargest
import heapq
print(heapq.nlargest(3, [5,1,3,7,2,9]))  // [9,7,5]

// Queue 生产者-消费者
import queue
q := queue.Queue(maxsize=5)
go func() {
    for i in range(10) {
        q.put(i)
    }
    q.put(nil)  // sentinel
}()
while true {
    item := q.get()
    if item == nil { break }
    print(item)
}
```
