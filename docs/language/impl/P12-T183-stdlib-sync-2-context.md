# P12-T183 stdlib: sync（Once / atomic）/ context

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

完成 `sync` 模块的 `Once`、`atomic` 操作，并实现 `context` 模块（取消、超时传播）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T182 | sync Mutex/WaitGroup |
| P12-T142 | time（context 超时） |

---

## API 清单

```ms
// sync.Once
once := sync.Once()
once.do(func)   // 确保 func 只执行一次（并发安全）

// sync.atomic
sync.atomic.load(ref) → value          // 原子读
sync.atomic.store(ref, value)          // 原子写
sync.atomic.add(ref, delta) → new_val  // 原子加
sync.atomic.cas(ref, old, new) → bool  // Compare-And-Swap
sync.atomic.swap(ref, new) → old       // 原子交换

// AtomicInt（类型化包装）
ai := sync.AtomicInt(0)
ai.load() → int
ai.store(v)
ai.add(delta) → int
ai.cas(old, new) → bool
ai.swap(new) → old

// context 模块
ctx := context.Background()       // 根 context（永不取消）
ctx := context.TODO()             // 占位 context

// 可取消 context
ctx, cancel := context.withCancel(parent)
cancel()    // 取消 ctx 及所有子 context

// 超时 context
ctx, cancel := context.withTimeout(parent, timeout_secs)
// deadline = now + timeout_secs
ctx, cancel := context.withDeadline(parent, deadline_dt)

// 值 context（传递请求范围数据）
ctx = context.withValue(parent, key, value)
ctx.value(key) → value  // 沿 context 链向上查找

// context 接口
ctx.done() → chan     // 取消时关闭的 channel
ctx.err() → error|nil  // nil=未取消; Canceled 或 DeadlineExceeded
ctx.deadline() → (datetime, bool)  // (deadline, has_deadline)

// 常量
context.Canceled        // ctx.err() 的取消原因
context.DeadlineExceeded  // 超时原因

// 用法：传递 context 给可取消操作
func doWork(ctx) {
    select {
        case <-ctx.done(): return ctx.err()
        case result <- compute():
            return result
    }
}
```

---

## 实现要点

```c
// sync.Once：
// state: 0=未执行 1=执行中 2=已完成
// do(f)：CAS(0→1) 成功则调用 f()，改 state=2；
//        若 state==1（执行中），yield 等待；若 state==2 直接返回

typedef struct MsOnceObj {
  MsObject       header;
  _Atomic int    state;    // 0/1/2
  MsCoroutineObj* waiters; // 等待 f() 完成的协程
} MsOnceObj;

// sync.AtomicInt：包裹 _Atomic int64_t
// msLang 不直接暴露 C 的 _Atomic，通过 MsAtomicIntObj 提供安全访问

// context 实现：
typedef struct MsContextObj {
  MsObject     header;
  MsContextObj* parent;    // 父 context
  MsChannelObj* done_ch;   // 取消时关闭
  MsValue      err;         // Canceled 或 DeadlineExceeded
  MsValue      key, val;    // withValue context 使用
  double       deadline;    // Unix 时间戳（-1=无 deadline）
  bool         canceled;
} MsContextObj;

// withCancel：创建新 ctx，注册到 parent 的子列表
// cancel()：关闭 done_ch（通知所有等待者），级联取消子 context

// withTimeout：创建 withCancel ctx + 注册定时器（time.after）
// 定时器到期时自动 cancel（err = DeadlineExceeded）

// done channel：关闭操作（close(ch)）唤醒所有 <-ch 等待者

// ctx.value(key)：沿 parent 链向上查找，直到 Background
```

---

## 验收标准（checklist）

- [ ] `Once.do(f)` 并发调用 100 次，f 只执行 1 次。
- [ ] `AtomicInt.add()` 并发 1000 协程各加 1 → 结果 1000。
- [ ] `withCancel` 取消后 `ctx.done()` channel 关闭。
- [ ] `withTimeout(ctx, 0.1)` 100ms 后自动取消（`ctx.err() = DeadlineExceeded`）。
- [ ] 子 context 在父取消时自动取消。
- [ ] `context.withValue` 沿链传播值。

---

## 测试用例（.ms）

```ms
import sync, context, time

// Once
once := sync.Once()
results := []
wg := sync.WaitGroup()
wg.add(100)
for _ in range(100) {
    go func() {
        once.do(lambda: results.append(1))
        wg.done()
    }()
}
wg.wait()
print(len(results))   // 1（只执行了一次）

// AtomicInt
ai := sync.AtomicInt(0)
wg2 := sync.WaitGroup()
wg2.add(1000)
for _ in range(1000) {
    go func() {
        ai.add(1)
        wg2.done()
    }()
}
wg2.wait()
print(ai.load())   // 1000

// context 超时
ctx, cancel := context.withTimeout(context.Background(), 0.1)
start := time.now()
<-ctx.done()   // 等待取消
elapsed := time.now() - start
print(ctx.err())        // context.DeadlineExceeded
print(elapsed > 0.09)   // true（≈100ms 后）
cancel()   // 无害地再次调用

// context 值传播
ctx2 := context.withValue(context.Background(), "user", "alice")
ctx3 := context.withValue(ctx2, "reqId", "12345")
print(ctx3.value("user"))   // "alice"（从父 context 传播）
print(ctx3.value("reqId"))  // "12345"
```
