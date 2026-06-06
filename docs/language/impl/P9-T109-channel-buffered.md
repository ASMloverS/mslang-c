# P9-T109 channel 有缓冲 / close / 迭代

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

扩展 channel 实现：支持有缓冲 channel（`make(chan, n)`），发送方在缓冲未满时不阻塞；实现 `close(ch)` 语义；支持 `for v in ch { }` 迭代（直到 channel 关闭）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T108 | 无缓冲 channel（MsChannelObj） |

---

## 实现要点

### 1. 有缓冲发送（OP_CHAN_SEND 扩展）

```c
case OP_CHAN_SEND: {
    MsValue val = POP();
    MsChannelObj* ch = (MsChannelObj*)MS_AS_OBJ(POP());
    if (ch->closed) return msRaiseRuntimeError(t, "send on closed channel");

    // 有接收者等待？直接交付（与无缓冲逻辑一致）
    if (ch->receivers) { /* ... rendezvous ... */ DISPATCH(); }

    // 有缓冲且未满：放入缓冲
    if (ch->cap > 0 && ch->len < ch->cap) {
        ch->buf[ch->tail] = val;
        ch->tail = (ch->tail + 1) % ch->cap;
        ch->len++;
        DISPATCH();  // 不阻塞！
    }

    // 缓冲满（或无缓冲且无接收者）：挂起
    MsWaiter* w = msAlloc(sizeof(MsWaiter));
    MsValue sendVal = val;
    w->coro = gScheduler.running;
    w->slot = &sendVal;
    w->next = NULL;
    appendWaiter(&ch->senders, w);
    msCoroYield();
    DISPATCH();
}
```

### 2. 有缓冲接收（OP_CHAN_RECV 扩展）

```c
case OP_CHAN_RECV: {
    MsChannelObj* ch = (MsChannelObj*)MS_AS_OBJ(POP());

    // 缓冲有数据？从缓冲取
    if (ch->len > 0) {
        MsValue val = ch->buf[ch->head];
        ch->head = (ch->head + 1) % ch->cap;
        ch->len--;
        // 唤醒等待发送者（缓冲腾出了空间）
        if (ch->senders) {
            MsWaiter* s = ch->senders;
            ch->senders = s->next;
            ch->buf[ch->tail] = *s->slot;
            ch->tail = (ch->tail + 1) % ch->cap;
            ch->len++;
            msSchedEnqueue(s->coro);
            msFree(s);
        }
        PUSH(val);
        DISPATCH();
    }

    // 无数据 + channel 已关闭
    if (ch->closed) { PUSH(MS_NIL_VAL); DISPATCH(); }

    // 有发送者等待？直接取（已在 T108 中处理）
    // ...（重用 T108 无缓冲逻辑）
}
```

### 3. `close(ch)`

```c
// close 内置函数（或 OP_CHAN_CLOSE）
static MsValue builtin_close(MsThread* t, MsValue* args, int argc) {
    if (argc != 1 || !MS_IS_OBJ(args[0]) || MS_AS_OBJ(args[0])->type != &msChanType)
        return msRaiseTypeError(t, "close() requires a channel");
    MsChannelObj* ch = (MsChannelObj*)MS_AS_OBJ(args[0]);
    if (ch->closed) return msRaiseRuntimeError(t, "close of closed channel");
    ch->closed = true;
    // 唤醒所有等待接收者（返回 nil）
    while (ch->receivers) {
        MsWaiter* r = ch->receivers;
        ch->receivers = r->next;
        *r->slot = MS_NIL_VAL;
        msSchedEnqueue(r->coro);
        msFree(r);
    }
    return MS_NIL_VAL;
}
```

### 4. `for v in ch { }` 迭代

```c
// channel 实现 tp_iter + tp_next
static MsValue chanIter(MsValue v) { return v; }  // channel 自身是迭代器
static MsValue chanNext(MsValue v) {
    MsChannelObj* ch = (MsChannelObj*)MS_AS_OBJ(v);
    // 缓冲有数据 → 取出
    if (ch->len > 0) { /* 从缓冲取值 */ }
    // channel 已关闭且无数据 → 迭代结束（返回 nil）
    if (ch->closed && ch->len == 0) return MS_NIL_VAL;
    // 等待：挂起当前协程，OP_FOR_ITER 会在恢复后再调用 chanNext
    // 这里把"接收等待"内联进 FOR_ITER
    /* ... */
}
```

---

## 验收标准（checklist）

- [ ] `make(chan, 5)` → 容量 5 的有缓冲 channel。
- [ ] 发送 5 个值到容量 5 的 channel，不阻塞；第 6 个发送时阻塞。
- [ ] `close(ch)` 后，所有等待接收者收到 `nil`。
- [ ] `close(ch)` 后，再次 close → `RuntimeError`。
- [ ] `for v in ch { }` 迭代直到 close，`v == nil` 停止。
- [ ] 向关闭 channel 发送 → `RuntimeError`。

---

## 测试用例（.ms）

```ms
// 有缓冲 channel
ch := make(chan, 3)
ch <- 1
ch <- 2
ch <- 3
print(<-ch)  // 1
print(<-ch)  // 2
print(<-ch)  // 3

// 生产者关闭 channel，消费者用 for 迭代
ch2 := make(chan, 10)
go func() {
    for i in range(5) { ch2 <- i }
    close(ch2)
}()

go func() {
    for v in ch2 {
        print(v)
    }
    print("done")
}()
// 输出：0 1 2 3 4 done
```

---

## Benchmark

```ms
// benchmarks/bench_chan_buf.ms
ch := make(chan, 1000)
n := 1_000_000
go func() { for i in range(n) { ch <- i } ; close(ch) }()
sum := 0
go func() { for v in ch { sum = sum + v } }()
// 目标：1M 有缓冲 < 1s（批量传输，context switch 较少）
```

---

## 风险与边界

- **`for v in ch` 与协程**：`OP_FOR_ITER` 调用 `tp_next`，若 channel 无数据且未关闭，`tp_next` 不能直接 yield（C 栈上）；需要特殊处理：在 `OP_FOR_ITER` 中检测 channel 类型并执行 yield，恢复后重试 `tp_next`。
