# P6-T081 处理器栈展开 + catch 类型匹配

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

完整实现异常传播时的**跨帧展开**（unwinding）：当当前帧无 ExceptEntry 时，依次弹出调用帧（关闭 upvalue），直到找到可以处理该异常的 ExceptEntry，或所有帧耗尽（未处理异常）。同时实现 `OP_ISINSTANCE` 驱动的 catch 类型匹配。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P6-T080 | raise 与 MS_ERROR_VALUE 传播 |
| P5-T078 | `OP_ISINSTANCE`（catch 类型检查） |
| P5-T071 | upvalue close（帧弹出时） |

---

## 实现要点

### 1. 跨帧展开循环

```c
// eval() 的 handle_error 区域（扩展 T080）
handle_error:
  while (t->exceptDepth > 0) {
    MsExceptEntry* entry = &t->exceptStack[t->exceptDepth - 1];

    // 展开到 entry->frame 所在的帧
    while (frame != entry->frame) {
      // 关闭当前帧的 upvalue
      msCloseUpvalues(t, frame->slots);
      // 执行 finally 块？（T082 处理；此处先跳过）
      msFreeFrame(frame);
      t->frame = frame = frame->caller;
      if (!frame) goto unhandled;
    }

    // 找到帧，跳转到 handler
    t->exceptDepth--;
    t->sp     = entry->savedSP;
    frame->ip = entry->handlerIP;
    PUSH(t->currentException);   // 异常对象在栈顶
    t->hasException = false;
    goto dispatch;
  }

unhandled:
  msPrintUnhandledException(t, stderr);
  return MS_ERROR_VALUE;
```

### 2. catch 类型匹配（由编译生成的 OP_ISINSTANCE 驱动）

catch 类型匹配在字节码层面已经由编译器（T046）生成 `OP_DUP + compileExpr(exc_type) + OP_ISINSTANCE + OP_POP_JUMP_FALSE` 序列；VM 层无需额外实现，只要 `OP_ISINSTANCE` 和 `OP_POP_JUMP_FALSE` 正确即可。

### 3. 多重 catch 链

```
// 字节码布局（handler 区）:
// 复制异常对象（DUP）
// 加载 ExcType1，ISINSTANCE，POP_JUMP_FALSE → next_handler
// (匹配)：POP（弹掉 DUP 的副本），绑定 e，body，JUMP end
// next_handler:
// 复制异常对象（DUP）
// 加载 ExcType2，ISINSTANCE，POP_JUMP_FALSE → reraise
// ...
// reraise:
// RERAISE（无匹配时重抛）
```

---

## 验收标准（checklist）

- [ ] try/catch 跨函数调用展开：`f()` 内 raise，外层 catch → 正确捕获。
- [ ] 多层调用栈展开（3+ 层）正确。
- [ ] catch 类型不匹配 → 继续找下一个 handler。
- [ ] 无匹配 handler → `RERAISE` 重抛，继续向外展开。
- [ ] 所有帧耗尽 → 未处理异常打印并退出。
- [ ] 展开时 upvalue 被正确关闭（捕获变量在堆上可访问）。

---

## 测试用例（.ms）

```ms
func inner() { raise ValueError("from inner") }
func middle() { inner() }
func outer() {
    try {
        middle()
    } catch ValueError as e {
        print("caught in outer:", e.message)  // caught in outer: from inner
    }
}
outer()

// 类型不匹配
try {
    raise ValueError("v")
} catch TypeError {
    print("TypeError")   // 不执行
} catch ValueError {
    print("ValueError")  // ← 此处
}
```

---

## Benchmark

N/A（异常路径非热路径）。

---

## 风险与边界

- **`finally` 与展开**：在展开过程中若途经含 `finally` 的帧，需先执行 finally 块再继续展开（T082 实现）。T081 先不处理 finally 展开（仅跳过）。
- **`ExceptEntry` 所属帧**：每个 `ExceptEntry` 记录了它所在的帧（`entry->frame`）；展开时跳过所有不属于该帧的 ExceptEntry。
