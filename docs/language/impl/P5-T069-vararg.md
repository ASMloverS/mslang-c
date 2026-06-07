# P5-T069 vararg（*args 参数收集）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

扩展调用约定以支持 `*args` 可变位置参数：函数定义中的 `*args` 参数将多余的位置参数收集为 tuple；调用时的 `*iterable` 展开（`OP_CALL_EX`）将可迭代对象展开为位置参数。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P5-T068 | 调用约定基础 |
| P4-T061 | tuple（vararg 收集结果为 tuple） |
| P4-T065 | 迭代协议（*iterable 展开） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §4.1 vararg 收集 |
| `syntax.md` | §3 函数参数语法 |

---

## 待实现（C 文件）

```
src/vm/ms_vm.c    # OP_CALL（扩展：hasVararg 分支）/ OP_CALL_EX
```

---

## 实现要点

### 1. 函数定义侧（收集 *args）

```c
// proto.hasVararg = true 时，arity = 必要参数数，arityMax = arity（不含 *args 槽）
// vararg 参数槽在 arityMax 位置

case OP_CALL: {
  // ... （基本参数绑定，见 T068）

  if (proto->hasVararg) {
    // 多余的位置参数打包为 tuple
    int varargCount = argc > (int)proto->arity ? argc - (int)proto->arity : 0;
    MsValue varargTuple = msNewTuple(varargCount);
    MsTupleObj* vt = (MsTupleObj*)MS_AS_OBJ(varargTuple);
    // 从栈上取 vararg 元素（arity 之后的参数）
    for (int i = 0; i < varargCount; i++) {
      vt->items[i] = frame->slots[proto->arity + i];
    }
    // 将 *args tuple 放入 arity 槽
    frame->slots[proto->arity] = varargTuple;
    // 栈指针回退到 arity+1（去掉多余参数）
    t->sp = frame->slots + proto->arity + 1;
  }
  // ...
}
```

### 2. 调用侧（*iterable 展开 → OP_CALL_EX）

```c
// OP_CALL_EX [1B: has_kwargs]
// 栈：[callee, args_list, kwargs_dict?]
case OP_CALL_EX: {
  uint8_t hasKwargs = READ_BYTE();
  MsValue kwargsDict = hasKwargs ? POP() : MS_NIL_VAL;
  MsValue argsSeq   = POP();
  MsValue callee    = PEEK(0);

  // 将 argsSeq 展开为单独参数压栈
  uint32_t argc = msExpandToStack(t, argsSeq);
  if (argc == UINT32_MAX) return MS_ERROR_VALUE;  // 非可迭代 → TypeError

  // 若有 kwargs，合并到调用（T070）
  if (hasKwargs) {
    return msCallWithKwargs(t, callee, argc, kwargsDict, frame);
  }
  // 重新走 OP_CALL 路径
  goto do_call;  // 伪代码；实际封装为 msCallFn(t, callee, argc)
}

// 将可迭代对象展开到栈，返回元素数量
static uint32_t msExpandToStack(MsThread* t, MsValue seq) {
  MsType* tp = msTypeOf(seq);
  if (!tp->tpIter) return UINT32_MAX;
  MsValue iter = tp->tpIter(seq);
  uint32_t count = 0;
  MsType* ti = msTypeOf(iter);
  for (;;) {
    MsValue v = ti->tpNext(iter);
    if (MS_IS_NIL(v)) break;
    PUSH(v);
    count++;
  }
  return count;
}
```

---

## 验收标准（checklist）

- [ ] `func f(*args) { return args }; f(1, 2, 3)` → `(1, 2, 3)`（tuple）。
- [ ] `func f(a, *args) { return a, args }; f(1, 2, 3)` → `1, (2, 3)`。
- [ ] `func f(*args) {}; f()` → args = `()`（空 tuple）。
- [ ] `f(*[1, 2, 3])` → 等价 `f(1, 2, 3)`（列表展开）。
- [ ] `f(*range(3))` → 等价 `f(0, 1, 2)`（range 展开）。
- [ ] `func f(a, *args) {}; f(*[1,2,3])` → a=1, args=(2,3)。

---

## 测试用例（.ms）

```ms
func sum(*nums) {
    s := 0
    for n in nums { s += n }
    return s
}
print(sum(1, 2, 3, 4, 5))   // 15

// 展开调用
lst := [10, 20, 30]
print(sum(*lst))              // 60

// 混合
func f(a, b, *rest) { return a + b + sum(*rest) }
print(f(1, 2, 3, 4))         // 10
```

---

## Benchmark

N/A（vararg 成本归入 T068 call bench）。

---

## 风险与边界

- **vararg 与默认值**：`func f(a, b=1, *args)` 中，*args 在所有位置参数之后；若 `f(10)` 调用，b=1（默认），args=()。
- **展开大型 iterable**：`f(*range(10M))` 会将 1000 万个值压栈，可能栈溢出；VM 未检查栈深度（初版不防护，文档提示）。
