# P9-T110 select 语句

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `select` 语句：同时等待多个 channel 操作，选择第一个就绪的执行；支持 `default` 分支（非阻塞）。语义对齐 Go 的 `select`。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T108 | 无缓冲 channel |
| P9-T109 | 有缓冲 channel + close |

---

## 实现要点

### 1. 编译器生成的 select 字节码

```
// select {
//   case v := <-ch1: body1
//   case ch2 <- x:   body2
//   default:         body3
// }
//
// 编译为：
//   OP_SELECT_BEGIN [nCases]
//   OP_SELECT_RECV [ch1_idx, result_local]
//   OP_SELECT_SEND [ch2_idx, val_expr_idx]
//   OP_SELECT_DEFAULT
//   OP_SELECT_END  [jumpTable: case_body_offset × nCases]
//   [case0_body, OP_JUMP end]
//   [case1_body, OP_JUMP end]
//   [default_body]
//   [end]
```

### 2. OP_SELECT_END 运行时

```c
case OP_SELECT_END: {
    uint8_t nCases = READ_BYTE();
    bool    hasDefault = READ_BYTE();
    // 读取跳转表
    uint16_t jumpOffsets[16];
    for (int i = 0; i < nCases + (hasDefault ? 1 : 0); i++)
        jumpOffsets[i] = READ_U16();

    // 收集 select 的 channel + 操作信息（由 OP_SELECT_RECV/SEND 压到辅助栈）
    SelectCase cases[16]; int nReady = 0;
    for (int i = 0; i < nCases; i++) {
        SelectCase* c = &cases[i];
        // 检查第 i 个 case 是否立即就绪
        if (c->isSend) {
            MsChannelObj* ch = (MsChannelObj*)MS_AS_OBJ(c->chan);
            if (ch->receivers || (ch->cap > 0 && ch->len < ch->cap))
                readyCases[nReady++] = i;
        } else {  // recv
            MsChannelObj* ch = (MsChannelObj*)MS_AS_OBJ(c->chan);
            if (ch->senders || ch->len > 0 || ch->closed)
                readyCases[nReady++] = i;
        }
    }

    if (nReady > 0) {
        // 随机选择一个就绪的（公平性）
        int chosen = readyCases[msRandUint() % nReady];
        // 执行对应的 channel 操作
        executeSelectCase(t, &cases[chosen]);
        // 跳转到对应 body
        t->frame->ip += jumpOffsets[chosen];
        DISPATCH();
    }

    if (hasDefault) {
        t->frame->ip += jumpOffsets[nCases];  // default body
        DISPATCH();
    }

    // 无 case 就绪，无 default：挂起，在所有 channel 上注册等待
    SelectWaiter* sw = allocSelectWaiter(t, cases, nCases, jumpOffsets);
    for (int i = 0; i < nCases; i++)
        registerSelectOnChannel(sw, &cases[i], i);
    msCoroYield();  // 等待任意一个 channel 就绪
    DISPATCH();
}
```

### 3. SelectWaiter（公平唤醒）

```c
// 当某个 channel 就绪时，找到对应的 SelectWaiter，取消其他 channel 上的注册，唤醒协程
typedef struct SelectWaiter {
    MsCoroutineObj* coro;
    SelectCase*     cases;
    int             nCases;
    uint16_t*       jumpOffsets;
    int             chosen;   // 被选中的 case 编号（就绪后设置）
    bool            resolved;
} SelectWaiter;
```

---

## 验收标准（checklist）

- [ ] 单个 case 就绪时正确执行。
- [ ] 多个 case 同时就绪时随机选一个（不每次选第一个）。
- [ ] `default` 分支在无 case 就绪时执行（非阻塞）。
- [ ] 无 case 就绪无 default → 协程挂起，直到某 channel 就绪。
- [ ] select 中的 recv 可绑定变量（`case v := <-ch`）。

---

## 测试用例（.ms）

```ms
// timeout 模式（配合 time.after）
ch := make(chan)
timeout := time.after(100)  // 100ms 后发送

go func() {
    select {
        case v := <-ch:
            print("received:", v)
        case <-timeout:
            print("timeout!")
    }
}()

// 不向 ch 发送，触发 timeout
// 期望输出：timeout!
```

```ms
// 多路复用
ch1 := make(chan)
ch2 := make(chan)

go func() { ch1 <- "from ch1" }()
go func() { ch2 <- "from ch2" }()

// 两次 select（收两个消息）
for i in range(2) {
    select {
        case v := <-ch1: print("ch1:", v)
        case v := <-ch2: print("ch2:", v)
    }
}
```

```ms
// default（非阻塞接收）
ch := make(chan)
select {
    case v := <-ch: print("got:", v)
    default:        print("no value ready")
}
// 输出：no value ready
```

---

## Benchmark

N/A（select 的性能依赖调度器，在 T114 综合 benchmark 中测）。

---

## 风险与边界

- **公平性**：多个 case 就绪时用 `rand` 选择，避免某个 channel 饥饿。需要一个轻量级伪随机数（LCG 即可，不需要密码学随机）。
- **取消注册竞争**：当多个 channel 同时就绪（T112 多线程后），需原子操作确保只有一个 SelectWaiter 被选中；初版单线程无此问题。
