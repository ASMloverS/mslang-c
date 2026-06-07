# P9-T111 Future / async func / await

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `async func` 声明和 `await` 表达式：`async func` 返回 `Future`（可等待对象）；`await` 挂起当前协程直到 Future 完成，并获取其结果。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T106 | 调度器 + MsCoroutineObj |
| P9-T107 | go 语句（异步函数也通过 go 启动） |
| P3-T045 | 编译器的 CALL_ASYNC 指令 |

---

## 实现要点

### 1. MsFutureObj 结构

```c
typedef struct MsFutureObj {
  MsObject      header;
  MsCoroutineObj* coro;   // 关联的协程
  MsValue       result;   // 完成后的返回值
  bool          done;
  MsWaiter*     awaiters; // 等待此 Future 的协程列表
} MsFutureObj;

MsType msFutureType = {
  .name = "Future",
  .tpMark = futureMark,
};
```

### 2. async func 调用（OP_CALL_ASYNC）

```c
// async func f() { ... } 编译为普通 MsFuncProto，isAsync=true
// 调用 async 函数时，编译器生成 OP_CALL_ASYNC 而非 OP_CALL

case OP_CALL_ASYNC: {
  uint8_t argc = READ_BYTE();
  MsValue fn   = PEEK(argc);

  // 创建 Future
  MsFutureObj* fut = msGCAlloc(sizeof(*fut), &msFutureType);
  fut->done = false;

  // 创建协程（复用 T107 的 msNewCoroutine）
  MsValue coro = msNewCoroutine(fn, t->sp - argc, argc);
  fut->coro = (MsCoroutineObj*)MS_AS_OBJ(coro);
  MsValue futVal = MS_OBJ_VAL((MsObject*)fut);

  // 协程完成时：通知 Future（注入回调）
  fut->coro->onDone = futureDoneCallback;
  fut->coro->onDoneArg = fut;

  // 加入调度器
  msSchedEnqueue(fut->coro);

  t->sp -= argc + 1;  // 弹出 fn + args
  PUSH(futVal);       // 返回 Future
  DISPATCH();
}

// 协程完成回调
static void futureDoneCallback(MsCoroutineObj* coro, void* arg) {
  MsFutureObj* fut = (MsFutureObj*)arg;
  fut->result = coro->result;
  fut->done   = true;
  // 唤醒所有等待者
  while (fut->awaiters) {
    MsWaiter* w = fut->awaiters;
    fut->awaiters = w->next;
    msSchedEnqueue(w->coro);
    msFree(w);
  }
}
```

### 3. OP_AWAIT

```c
case OP_AWAIT: {
  MsValue futVal = POP();
  if (!MS_IS_OBJ(futVal) || MS_AS_OBJ(futVal)->type != &msFutureType)
    return msRaiseTypeError(t, "await requires a Future");
  MsFutureObj* fut = (MsFutureObj*)MS_AS_OBJ(futVal);

  if (fut->done) {
    // 已完成：直接取结果
    PUSH(fut->result);
    DISPATCH();
  }

  // 未完成：挂起当前协程，注册到 Future.awaiters
  MsWaiter* w = msAlloc(sizeof(*w));
  w->coro = gScheduler.running;
  w->next = NULL;
  appendWaiter(&fut->awaiters, w);
  gScheduler.running->awaitingFuture = futVal;
  msCoroYield();
  // 恢复后：Future 已完成
  PUSH(fut->result);
  DISPATCH();
}
```

### 4. async/await 使用示例

```ms
async func fetchData(url) {
    // 模拟异步操作（实际会 await 底层 I/O Future）
    await time.sleep(10)  // await MsFutureObj
    return "data from " + url
}

async func main() {
    f1 := fetchData("http://a.com")  // OP_CALL_ASYNC → Future
    f2 := fetchData("http://b.com")
    // 并发执行：两个 fetch 同时在调度器中运行
    r1 := await f1
    r2 := await f2
    print(r1)
    print(r2)
}
```

---

## 验收标准（checklist）

- [ ] `async func f()` 返回 `Future` 对象。
- [ ] `await future` 在 Future 完成前挂起当前协程。
- [ ] Future 完成后，等待协程恢复并获取结果。
- [ ] 多个协程可 await 同一个 Future。
- [ ] `await` 在非 async 函数中使用 → 编译错误（T045 编译时检查）。
- [ ] Future 内异常 → `await` 重新抛出异常到等待方。

---

## 测试用例（.ms）

```ms
async func double(x) {
    return x * 2
}

async func run() {
    f := double(21)
    print(type(f))   // Future
    v := await f
    print(v)         // 42
}

go run()
```

```ms
// 并发多个 async
async func compute(n) {
    s := 0
    for i in range(n) { s = s + i }
    return s
}

async func main() {
    futs := [compute(1000), compute(2000), compute(3000)]
    for f in futs {
        print(await f)
    }
}
go main()
```

---

## Benchmark

```ms
// 1000 个并发 async 函数
async func worker(n) { return n * n }

async func bench() {
    futs := []
    for i in range(1000) { futs.append(worker(i)) }
    results := []
    for f in futs { results.append(await f) }
    print("done:", len(results))
}
go bench()
// 目标 < 100ms（调度 1000 个短协程）
```

---

## 风险与边界

- **await 只能在 async 函数内使用**：编译器（T045）在 `compileFunc` 时设 `isAsync` 标志，遇到 `await` 检查当前是否在 async 函数中，否则 → 编译错误。
- **Future 异常传播**：若 async 函数内部异常未被捕获，Future 完成时携带异常；`await` 恢复后重新抛出（设 `t->currentException`，返回 `MS_ERROR_VALUE`）。
