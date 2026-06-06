# P6-T080 raise / reraise + MS_ERROR_VALUE 传播

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `OP_RAISE`/`OP_RERAISE` 指令的完整语义：设置当前线程的异常状态（`t->currentException`）、从错误返回 `MS_ERROR_VALUE`，以及 VM 主循环对 `MS_ERROR_VALUE` 的拦截与异常传播机制（展开调用帧）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P6-T079 | 异常类层次 |
| P5-T068 | 调用约定（帧结构） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `errors.md` | §2 异常传播机制 |
| `vm.md` | §6 异常处理器栈 |

---

## 实现要点

### 1. 线程异常状态

```c
typedef struct MsThread {
    // ... （现有字段）
    bool      hasException;
    MsValue   currentException;  // 当前传播的异常对象
    MsValue   exceptionCause;    // raise X from Y 中的 Y
} MsThread;
```

### 2. OP_RAISE

```c
case OP_RAISE: {
    // 栈：[exc_obj, cause]（cause 可为 nil）
    MsValue cause  = POP();
    MsValue excObj = POP();

    // 验证是异常实例
    if (!MS_IS_OBJ(excObj) || !msIsExceptionInstance(excObj)) {
        // 若 excObj 是异常类，自动实例化（与 Python 一致）
        if (MS_IS_OBJ(excObj) && MS_AS_OBJ(excObj)->type == &msMetaType) {
            excObj = msCallFn(t, excObj, NULL, 0);  // ExcClass()
            if (MS_IS_ERROR(excObj)) goto propagate;
        } else {
            return msTypeError(t, "exceptions must derive from BaseException");
        }
    }

    t->currentException = excObj;
    t->exceptionCause   = cause;
    t->hasException     = true;
    return MS_ERROR_VALUE;
}
```

### 3. OP_RERAISE

```c
case OP_RERAISE:
    // 重新抛出当前异常（保留 currentException 不变）
    if (!t->hasException) {
        // 无当前异常 → RuntimeError
        t->currentException = msNewException(gVM.RuntimeError, "no active exception");
        t->hasException = true;
    }
    return MS_ERROR_VALUE;
```

### 4. VM 主循环的异常传播

```c
// eval() 中每条可能产生错误的指令后：
// MS_ERROR_VALUE 从操作函数返回 → eval() 直接返回给调用者
// 调用者（OP_CALL 的 VM 实现）检测到 MS_ERROR_VALUE：

case OP_CALL: {
    // ...
    MsValue result = msCallFn(t, callee, args, argc);
    t->sp -= argc + 1;
    if (MS_IS_ERROR(result)) goto propagate;  // 传播到调用者的处理器
    PUSH(result);
    DISPATCH();
}

propagate:
    // 在当前帧链中向上找 ExceptEntry（OP_PUSH_EXCEPT 注册的处理器）
    return MS_ERROR_VALUE;  // 传播给外层 eval 调用
```

### 5. 异常处理器栈（ExceptEntry）

```c
typedef struct MsExceptEntry {
    uint8_t*  handlerIP;  // 处理器代码位置（OP_PUSH_EXCEPT 的目标）
    MsValue*  savedSP;    // 压 handler 时的栈指针（恢复用）
    MsFrame*  frame;      // 所在帧
} MsExceptEntry;

// 每个线程维护 ExceptEntry 栈
typedef struct MsThread {
    // ...
    MsExceptEntry exceptStack[256];
    int           exceptDepth;
} MsThread;

case OP_PUSH_EXCEPT: {
    uint16_t offset = READ_U16();
    MsExceptEntry* entry = &t->exceptStack[t->exceptDepth++];
    entry->handlerIP = frame->ip + offset;  // handler 代码位置
    entry->savedSP   = t->sp;
    entry->frame     = frame;
    DISPATCH();
}

case OP_POP_EXCEPT:
    if (t->exceptDepth > 0) {
        t->exceptDepth--;
        // 若有当前异常（正常退出后）清除
        // 注意：正常退出不清除 hasException（hasException=false 时 POP_EXCEPT 是 no-op）
    }
    DISPATCH();
```

### 6. propagate 跳转（处理器查找）

当 eval 收到 `MS_ERROR_VALUE` 时，在主循环末尾：

```c
// 在 eval() 的 switch 之后（goto 目标）
handle_error:
    while (t->exceptDepth > 0) {
        MsExceptEntry* entry = &t->exceptStack[--t->exceptDepth];
        if (entry->frame == frame) {
            // 在当前帧找到处理器
            t->sp     = entry->savedSP;
            frame->ip = entry->handlerIP;
            // 将异常对象压栈（处理器代码期望栈顶为异常对象）
            PUSH(t->currentException);
            t->hasException = false;
            goto dispatch;
        } else {
            // 跨帧展开：恢复外层帧（T081 进一步实现）
            msCloseUpvalues(t, entry->frame->slots);
            msFreeFrame(frame);
            frame = t->frame = entry->frame;
            // ...
        }
    }
    // 无处理器 → 未处理异常，打印 traceback 并退出
    msPrintTraceback(t, stderr);
    return MS_ERROR_VALUE;
```

---

## 验收标准（checklist）

- [ ] `raise TypeError("x")` → 设置 `t->currentException`，返回 `MS_ERROR_VALUE`。
- [ ] `raise TypeError` （类名，非实例）→ 自动 `TypeError()` 实例化。
- [ ] `raise` 在 catch 内 → 重抛当前异常。
- [ ] `raise` 在 catch 外 → RuntimeError。
- [ ] `raise ValueError from KeyError` → `t->exceptionCause` 被设置。
- [ ] `1 / 0` → 内部 `msRaiseZeroDivisionError`，走相同传播路径。
- [ ] 未捕获异常 → 打印类型名和消息到 stderr，进程退出 1。

---

## 测试用例（.ms）

```ms
// 基础 raise/catch
try {
    raise ValueError("bad input")
} catch ValueError as e {
    print("caught:", e.message)   // caught: bad input
}

// raise from（原因链）
try {
    try {
        x := int("abc")
    } catch ValueError as e {
        raise RuntimeError("parse failed") from e
    }
} catch RuntimeError as e {
    print(e.message)           // parse failed
    // print(e.__cause__)     // ValueError: invalid literal...（T083 完整支持）
}

// 未捕获（输出到 stderr）
// raise TypeError("oops")   → stderr: TypeError: oops
```

---

## Benchmark

N/A（异常传播是非常规路径，不测试吞吐量）。

---

## 风险与边界

- **每条指令后检查 `hasException`**：与 Java/C++ 不同，mslang 使用返回值（`MS_ERROR_VALUE`）而非 `setjmp/longjmp`；每个可失败指令必须在返回 `MS_ERROR_VALUE` 后由 VM 主循环跳转到 `handle_error`。此方案避免了 C 栈展开的开销，但要求所有指令实现都返回 `MS_ERROR_VALUE`（而非直接调用 `longjmp`）。
- **`ExceptEntry` 跨帧展开**：T081 完整实现；T080 只处理"单帧内找到处理器"的情况。
