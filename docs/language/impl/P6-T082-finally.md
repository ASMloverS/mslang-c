# P6-T082 finally 多路径语义

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

确保 `finally` 块在**所有**退出路径上执行：正常退出、catch 成功、异常传播（catch 失败）、`return`、`break`、`continue`。编译器（T046）已内联 finally，本任务完善运行时保证（特别是 `return`/`break` 路径）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P6-T081 | 异常展开 |
| P3-T046 | finally 编译（内联） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `errors.md` | §4 finally 语义 |
| `vm.md` | §6 异常机制（ExceptEntry 展开） |

---

## 实现要点

### 1. finally 内联（已有，T046）

编译器将 finally 块内联到每条退出路径，因此大多数情况无需 VM 特殊处理。

### 2. return 内的 finally

当函数体中有 `try { return x } finally { f() }` 时，编译器生成：

```
compileExpr(x)      → 栈顶：x（待返回值）
OP_PUSH_EXCEPT      → 注册 handler（处理 finally 的异常路径）
compile(return x)   → 在 OP_RETURN 之前插入 finally
OP_POP_EXCEPT
[finally body]
OP_RETURN

[handler]:
[finally body（内联复制）]
OP_RERAISE
```

这由编译器（T046）生成，VM 层面透明。

### 3. with 语句的 finally 语义（OP_WITH_EXIT）

```c
// OP_WITH_EXIT [1B: is_error]
// is_error=0：正常退出，调用 ctx.__exit__(nil, nil, nil)
// is_error=1：异常退出，调用 ctx.__exit__(exc_type, exc_val, tb)
// 若 __exit__ 返回 true → 吞掉异常（POP_EXCEPT）
// 若 __exit__ 返回 false/nil → 重抛（RERAISE）

case OP_WITH_EXIT: {
  uint8_t isError = READ_BYTE();
  MsValue ctxMgr  = PEEK(0);  // 上下文管理器在栈上的位置（编译器安排）

  MsValue exitMethod = msGetAttr(ctxMgr, msInternStr("__exit__"));
  if (MS_IS_NIL(exitMethod) || MS_IS_ERROR(exitMethod)) {
    return msAttributeError(t, "__exit__");
  }

  MsValue args[3];
  if (isError) {
    MsValue exc = t->currentException;
    args[0] = MS_OBJ_VAL(msTypeOf(exc) == &msInstanceType
                  ? (MsObject*)((MsInstanceObj*)MS_AS_OBJ(exc))->klass
                  : (MsObject*)NULL);
    args[1] = exc;
    args[2] = MS_NIL_VAL;   // traceback（T083 后填充）
  } else {
    args[0] = args[1] = args[2] = MS_NIL_VAL;
  }
  MsValue result = msCallFn(t, exitMethod, args, 3);
  if (MS_IS_ERROR(result)) return result;

  if (isError) {
    if (msValueTruthy(result)) {
      // 吞掉异常
      t->hasException = false;
      t->currentException = MS_NIL_VAL;
    }
    // 否则：异常继续传播（handle_error 会处理）
  }
  DISPATCH();
}
```

---

## 验收标准（checklist）

- [ ] 正常退出时 finally 执行。
- [ ] 异常退出时 finally 执行后异常继续传播（未被 with 吞掉）。
- [ ] `return x` 在 try 内 → finally 执行，再 return。
- [ ] `with ctx as c { return v }` → `__exit__(nil,nil,nil)` 被调用，再 return。
- [ ] `__exit__` 返回 true → 异常被吞掉（不传播）。
- [ ] `__exit__` 返回 false → 异常继续传播。

---

## 测试用例（.ms）

```ms
// finally 保证执行
func f() {
    try {
        return 42
    } finally {
        print("finally!")   // 一定打印
    }
}
print(f())   // finally!\n42

// with 语句
class CM {
    func __enter__(self) { print("enter"); return self }
    func __exit__(self, *args) { print("exit"); return false }
}

with CM() as c {
    print("body")
    raise ValueError("oops")
}
// enter
// body
// exit
// → ValueError 继续传播（__exit__ 返回 false）

// 吞掉异常
class SuppressCM {
    func __exit__(self, *args) { return true }  // 吞掉
}
try {
    with SuppressCM() {
        raise ValueError("x")
    }
    print("no exception!")   // 会打印
} catch Exception {
    print("unreachable")
}
```

---

## Benchmark

N/A（finally 在常规代码中几乎无开销，内联消除了运行时检查）。

---

## 风险与边界

- **finally 内联膨胀**：大型 finally 块（数十行代码）被内联到所有退出路径，字节码体积可观；极端情况考虑 jsr/ret 子程序化，但初版接受内联方案。
- **`__enter__` 失败**：若 `__enter__` 抛出异常，`__exit__` 不会被调用（与 Python 一致）；`OP_WITH_ENTER` 在异常时直接传播，不压 ExceptEntry。
