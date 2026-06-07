# P9-T108 channel 无缓冲（rendezvous）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现无缓冲 channel：发送方（`ch <- val`）与接收方（`val := <-ch`）必须同步等待对方就位（rendezvous），通过调度器实现协程切换而不阻塞 OS 线程。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T106 | 调度器 + 协程 |
| P9-T107 | go 语句 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `concurrency.md` | §2 channel |

---

## 实现要点

### 1. MsChannelObj 结构

```c
typedef struct MsWaiter {
  MsCoroutineObj* coro;  // 等待中的协程
  MsValue*        slot;  // 发送：slot=&value；接收：slot=接收位置
  struct MsWaiter* next;
} MsWaiter;

typedef struct MsChannelObj {
  MsObject  header;
  MsValue*  buf;         // 缓冲区（无缓冲时为 NULL）
  uint32_t  cap;         // 容量（无缓冲=0）
  uint32_t  head, tail;  // 缓冲区读写指针
  uint32_t  len;         // 当前缓冲数量
  bool      closed;
  MsWaiter* senders;     // 等待发送的协程队列
  MsWaiter* receivers;   // 等待接收的协程队列
} MsChannelObj;

MsType msChanType = {
  .name = "channel",
  .tpMark = chanMark,
  .tpFree = chanFree,
};

// make(chan T) → 无缓冲 channel
MsValue msNewChan(uint32_t capacity) {
  MsChannelObj* ch = msGCAlloc(sizeof(*ch), &msChanType);
  ch->cap  = capacity;
  ch->buf  = capacity ? msAlloc(capacity * sizeof(*ch->buf)) : NULL;
  ch->closed = false;
  ch->senders = ch->receivers = NULL;
  return MS_OBJ_VAL((MsObject*)ch);
}
```

### 2. OP_CHAN_SEND（`ch <- val`）

```c
case OP_CHAN_SEND: {
  MsValue val = POP();
  MsValue chv = POP();  // channel
  MsChannelObj* ch = (MsChannelObj*)MS_AS_OBJ(chv);

  if (ch->closed) return msRaiseRuntimeError(t, "send on closed channel");

  // 有等待接收者？直接 rendezvous
  if (ch->receivers) {
    MsWaiter* r = ch->receivers;
    ch->receivers = r->next;
    *r->slot = val;  // 把值放到接收者的槽
    msSchedEnqueue(r->coro);  // 唤醒接收者
    msFree(r);
    DISPATCH();
  }

  // 无缓冲且无接收者：挂起当前协程
  MsWaiter* w = msAlloc(sizeof(*w));
  MsValue   sendVal = val;
  w->coro = gScheduler.running;
  w->slot = &sendVal;
  w->next = NULL;
  // 链入 senders 队列（尾插）
  appendWaiter(&ch->senders, w);
  msCoroYield();  // 切回调度器，等待接收者
  DISPATCH();
}
```

### 3. OP_CHAN_RECV（`val := <-ch`）

```c
case OP_CHAN_RECV: {
  MsValue chv = POP();
  MsChannelObj* ch = (MsChannelObj*)MS_AS_OBJ(chv);

  // 有等待发送者？直接 rendezvous
  if (ch->senders) {
    MsWaiter* s = ch->senders;
    ch->senders = s->next;
    MsValue val = *s->slot;
    msSchedEnqueue(s->coro);  // 唤醒发送者
    msFree(s);
    PUSH(val);
    DISPATCH();
  }

  // channel 已关闭且无发送者
  if (ch->closed) {
    PUSH(MS_NIL_VAL);  // 关闭的 channel 返回零值
    DISPATCH();
  }

  // 无发送者：挂起当前协程
  MsValue recvSlot = MS_NIL_VAL;
  MsWaiter* w = msAlloc(sizeof(*w));
  w->coro = gScheduler.running;
  w->slot = &recvSlot;
  w->next = NULL;
  appendWaiter(&ch->receivers, w);
  msCoroYield();  // 等待发送者
  PUSH(recvSlot);
  DISPATCH();
}
```

---

## 验收标准（checklist）

- [ ] 发送方在接收方未就位时挂起（不死锁）。
- [ ] 接收方在发送方未就位时挂起。
- [ ] 发送方先就位，接收方到达 → 立即 rendezvous。
- [ ] 接收方先就位，发送方到达 → 立即 rendezvous。
- [ ] 向已关闭 channel 发送 → `RuntimeError`。
- [ ] 从已关闭 channel 接收 → 返回 `nil`。

---

## 测试用例（.ms）

```ms
// ping-pong
ch := make(chan)

go func() {
    ch <- "ping"
    msg := <-ch
    print("got:", msg)
}()

go func() {
    msg := <-ch
    print("got:", msg)
    ch <- "pong"
}()

// 输出（顺序确定）：
// got: ping
// got: pong
```

```ms
// 生产者-消费者（无缓冲）
ch := make(chan)

go func() {
    for i in range(5) {
        ch <- i
    }
}()

go func() {
    for i in range(5) {
        v := <-ch
        print(v)
    }
}()
// 输出：0 1 2 3 4
```

---

## Benchmark

```ms
// benchmarks/bench_chan_unbuf.ms
ch := make(chan)
n := 100_000
go func() {
    for i in range(n) { ch <- i }
}()
sum := 0
go func() {
    for i in range(n) { sum = sum + (<-ch) }
}()
// 目标：100K rendezvous < 500ms（单线程协作）
```

---

## 风险与边界

- **死锁检测**：若所有协程都在等待 channel（无发送方/接收方就位），调度器 `ready` 队列为空但有阻塞的协程 → 死锁。初版只打印 "all goroutines are asleep - deadlock!" 并退出（与 Go 运行时一致）。
