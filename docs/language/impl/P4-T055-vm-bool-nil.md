# P4-T055 bool / nil 类型 + 真值测试指令

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `bool` 和 `nil` 运行时类型，以及 `OP_NOT`（逻辑非）、`OP_IS_TRUE`（条件跳转前真值提取）等基础指令。`bool` 只有 `true`/`false` 两个值（不分配堆对象），`nil` 是单例。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T051 | 求值循环 |
| P4-T049 | `MsValue` 定义（`MS_BOOL_VAL`/`MS_NIL_VAL`） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §6 bool/nil 类型 |
| `syntax.md` | §2.4 逻辑运算符（and/or/not，Python 语义） |

---

## 待实现（C 文件）

```
src/runtime/ms_bool.c    # msBoolType / msNilType
include/mslang/ms_bool.h
```

---

## 实现要点

### 1. bool 类型槽

```c
static MsValue boolRepr(MsValue v) {
  return MS_AS_BOOL(v) ? msNewStr("true", 4) : msNewStr("false", 5);
}

static MsValue boolEq(MsValue a, MsValue b) {
  // bool == bool
  if (MS_IS_BOOL(b)) return MS_BOOL_VAL(MS_AS_BOOL(a) == MS_AS_BOOL(b));
  // bool == int（true==1, false==0，兼容 Python）
  if (MS_IS_INT(b))  return MS_BOOL_VAL((int64_t)MS_AS_BOOL(a) == MS_AS_INT(b));
  return MS_BOOL_VAL(false);
}

static MsValue boolHash(MsValue v) {
  return MS_INT_VAL(MS_AS_BOOL(v) ? 1 : 0);
}

MsType msBoolType = {
  .name = "bool", .instanceSize = 0,
  .tpRepr = boolRepr, .tpStr = boolRepr,
  .tpHash = boolHash, .tpEq  = boolEq,
};
```

### 2. nil 类型槽

```c
static MsValue nilRepr(MsValue v) { (void)v; return msNewStr("nil", 3); }
static MsValue nilEq(MsValue a, MsValue b) {
  (void)a; return MS_BOOL_VAL(MS_IS_NIL(b));
}
static MsValue nilHash(MsValue v) { (void)v; return MS_INT_VAL(0); }

MsType msNilType = {
  .name = "nil", .instanceSize = 0,
  .tpRepr = nilRepr, .tpStr = nilRepr,
  .tpHash = nilHash, .tpEq  = nilEq,
};
```

### 3. VM 指令

```c
case OP_NOT: {
  MsValue v = POP();
  PUSH(MS_BOOL_VAL(!msValueTruthy(v)));
  DISPATCH();
}

// 条件跳转（POP_JUMP_FALSE / POP_JUMP_TRUE）
case OP_POP_JUMP_FALSE: {
  uint16_t offset = READ_U16();
  MsValue v = POP();
  if (!msValueTruthy(v)) frame->ip += offset;
  DISPATCH();
}
case OP_POP_JUMP_TRUE: {
  uint16_t offset = READ_U16();
  MsValue v = POP();
  if (msValueTruthy(v)) frame->ip += offset;
  DISPATCH();
}
// 不弹出版本（用于 and/or 短路）
case OP_JUMP_FALSE: {
  uint16_t offset = READ_U16();
  if (!msValueTruthy(PEEK(0))) frame->ip += offset;
  DISPATCH();
}
case OP_JUMP_TRUE: {
  uint16_t offset = READ_U16();
  if (msValueTruthy(PEEK(0))) frame->ip += offset;
  DISPATCH();
}
case OP_JUMP: {
  uint16_t offset = READ_U16();
  frame->ip += offset;
  DISPATCH();
}
case OP_LOOP: {
  uint16_t offset = READ_U16();
  frame->ip -= offset;
  DISPATCH();
}
```

### 4. 真值规则（`msValueTruthy`）

```c
bool msValueTruthy(MsValue v) {
  switch (v.tag) {
  case MS_TAG_NIL:   return false;
  case MS_TAG_BOOL:  return MS_AS_BOOL(v);
  case MS_TAG_INT:   return MS_AS_INT(v) != 0;
  case MS_TAG_FLOAT: return MS_AS_FLOAT(v) != 0.0 && !isnan(MS_AS_FLOAT(v));
  case MS_TAG_OBJ: {
    MsObject* obj = MS_AS_OBJ(v);
    MsType* tp = obj->type;
    if (tp->tpLen) {
      MsValue len = tp->tpLen(v);
      return MS_AS_INT(len) != 0;
    }
    if (tp->tpHash) return true;  // 自定义对象默认为真
    return true;
  }
  default: return true;
  }
}
```

---

## 验收标准（checklist）

- [ ] `not true` → false；`not false` → true。
- [ ] `not nil` → true；`not 0` → true；`not 1` → false。
- [ ] `not []`（空列表，T059 实现后）→ true。
- [ ] `if 0 { ... }` 不进入 body。
- [ ] `if nil { ... }` 不进入 body。
- [ ] `true == 1` → true（bool 与 int 兼容）。
- [ ] `nil == nil` → true；`nil == false` → false。
- [ ] `repr(true)` → `"true"`；`repr(nil)` → `"nil"`。
- [ ] 短路 `and`：`false and sideEffect()` → 不执行 `sideEffect()`。
- [ ] 短路 `or`：`true or sideEffect()` → 不执行 `sideEffect()`。

---

## 测试用例（C 单测）

### `tests/vm/test_bool_nil.c`

```c
#include "ms_test.h"
#include "mslang/ms_vm.h"
#include "mslang/ms_compiler.h"

static MsValue run(const char* src) {
  MsCompileResult r = msCompile(src, strlen(src), "<t>");
  msVMInit();
  MsValue v = msVMRun(r.chunk);
  msVMShutdown();
  msCompileResultFree(&r);
  return v;
}

static void testNot(void) {
  MsValue v = run("not true");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "not true = false");
  v = run("not 0");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v),  "not 0 = true");
}

static void testShortCircuit(void) {
  // "false and (1/0)" → 不执行除法，不崩溃
  MsValue v = run("false and (1/0)");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "false and short-circuit");
  // "true or (1/0)" → 不执行除法
  v = run("true or (1/0)");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "true or short-circuit");
}

int main(void) {
  MS_RUN(testNot);
  MS_RUN(testShortCircuit);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
// bool 运算
print(true and false)  // false
print(true or false)   // true
print(not true)        // false

// nil 处理
x := nil
if x == nil {
    print("x is nil")   // x is nil
}

// 真值（falsy/truthy）
vals := [nil, false, 0, 0.0, "", [], {}, ()]
for v in vals {
    print(v, "→", bool(v))  // 全部为 false（T097 bool() 内置）
}

// and/or 返回操作数本身（Python 语义）
print(1 and 2)    // 2（第一个为真，返回第二个）
print(0 and 2)    // 0（第一个为假，返回第一个）
print(nil or 42)  // 42（第一个为假，返回第二个）
```

---

## Benchmark

N/A（分支预测相关，在整体循环 bench 中体现）。

---

## 风险与边界

- **`and`/`or` 返回值**：编译器（T039）已用短路跳转模式，结果为最后被求值的操作数（不强转 bool），与 Python 语义一致：`1 and 2` → 2，`0 or "x"` → "x"。
- **`nan` 真值**：`float('nan')` 真值为 false（`isnan` 返回 true → 处理为 false，与 Python 一致）。
- **`bool` 是 `int` 子类**（Python）：mslang v1 中 `bool` 独立类型（不继承 int），但 `true + 1` 可考虑允许（初版暂不支持，报 TypeError）。
