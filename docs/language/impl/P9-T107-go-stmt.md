# P9-T107 go 语句运行期

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `go funcCall()` 语句的完整运行期：将函数调用封装为协程对象，加入调度器就绪队列；函数执行完毕后协程变为 DONE 状态。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P9-T106 | MsCoroutineObj + 调度器 |
| P5-T068 | 调用约定 |

---

## 实现要点

### 1. OP_GO 指令

```c
// 编译器（T047）为 `go f(args)` 生成：
//   [push args]
//   OP_CALL [argc]   → 但这里不调用！编译器特殊处理
//
// 实际：T047 为 go 生成 OP_GO [argc]，保留栈顶为 func + args

case OP_GO: {
  uint8_t argc = READ_BYTE();
  MsValue func = PEEK(argc);  // 函数在 args 下面

  // 弹出 func + args
  MsValue* args = t->sp - argc;
  MsValue  fn   = *(args - 1);
  t->sp -= argc + 1;

  // 创建协程
  MsValue coro = msNewCoroutine(fn, args, argc);
  if (MS_IS_ERROR(coro)) return coro;

  // 加入调度器
  msSchedEnqueue((MsCoroutineObj*)MS_AS_OBJ(coro));
  DISPATCH();
}
```

### 2. msNewCoroutine

```c
MsValue msNewCoroutine(MsValue fn, MsValue* args, int argc) {
  MsCoroutineObj* coro = msGCAlloc(sizeof(*coro), &msCoroutineType);
  coro->state     = CORO_CREATED;
  coro->result    = MS_NIL_VAL;
  coro->exception = MS_NIL_VAL;

  // 分配独立 MsThread（有自己的栈）
  coro->thread = msAllocThread(CORO_STACK_SIZE);
  coro->thread->globals = gVM.mainThread.globals;  // 共享全局（初版）

  // 在协程栈上准备初始帧（调用 fn(args)）
  msThreadPrepareCall(coro->thread, fn, args, argc);

  // 初始化上下文（ucontext / Fiber）
  msCoroInitCtx(coro, coroEntry);

  return MS_OBJ_VAL((MsObject*)coro);
}

// 协程入口（在协程栈上执行）
static void coroEntry(MsCoroutineObj* coro) {
  MsValue result = eval(coro->thread);
  coro->result = result;
  coro->state  = CORO_DONE;
  // 切回调度器
  msCoroYieldToSched();
}
```

### 3. 主线程等待所有协程完成

```c
// 在 main 函数或模块顶层执行完毕后，运行调度器：
// msSchedRun() 直到 gScheduler.count == 0
//
// mslang run foo.ms 伪代码：
//   msLoadAndExec("foo.ms");      // 执行顶层代码（可能 go 一些协程）
//   msSchedRun();                 // 等待所有协程完成
```

---

## 验收标准（checklist）

- [ ] `go f()` → f 在协程中运行，不阻塞当前执行。
- [ ] 顶层代码执行完后，调度器运行所有已入队协程。
- [ ] 协程内 `return` 使协程变 DONE，结果存在 `coro.result`。
- [ ] 协程内未捕获异常 → 打印错误（不杀死主程序）。
- [ ] `go` 可嵌套：协程内再 `go` 新协程。

---

## 测试用例（.ms）

```ms
// 基础 go
results := []
for i in range(5) {
    go func(n) {
        results.append(n * n)
    }(i)
}
// 主线程继续执行（协程还未运行）
print("main after go")

// 主线程结束后调度器启动协程
// results 会被填充 [0,1,4,9,16]（顺序不确定）
```

```ms
// 协程内异常隔离
go func() {
    raise ValueError("coro error")
}()

go func() {
    print("I still run!")  // 不受上一个协程影响
}()
// stderr: ValueError: coro error
// stdout: I still run!
```

---

## Benchmark

```ms
// 1000 个协程并发
n := 1000
for i in range(n) {
    go func(x) {
        // 简单计算
        s := 0
        for j in range(1000) { s = s + j }
        return s
    }(i)
}
// 目标：1000 协程 × 1000 迭代 < 2s（单线程协作调度）
```

---

## 风险与边界

- **共享全局**：初版协程共享 `globals`（主线程的 MsMapObj）。由于单线程协作调度，无数据竞争；M:N 多线程（T112）后需锁保护全局访问。
- **协程 GC**：`MsCoroutineObj` 是 GC 管理对象；调度器内的 ready 队列是 GC 根；协程的 `MsThread` 及其栈也是 GC 根（需在 markRoots 中枚举）。
