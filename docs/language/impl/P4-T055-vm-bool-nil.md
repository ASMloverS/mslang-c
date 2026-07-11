# P4-T055 bool / nil 类型 + 真值测试指令

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `bool` 和 `nil` 运行时类型，以及 `OP_NOT`（逻辑非）、条件跳转（`OP_JUMP_IF_FALSE`/`OP_JUMP_IF_TRUE`）、短路跳转（`OP_AND_JMP`/`OP_OR_JMP`）等基础指令。`bool` 只有 `true`/`false` 两个值（不分配堆对象），`nil` 是单例。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T051 | 求值循环 |
| P4-T049 | `MsValue` 定义（`MS_BOOL_VAL`/`MS_NIL_VAL`） |
| P4-T050 | GC/`msNewStr`（供 `repr` 分配 str） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §2.3 bool；§2.4 nil；§1.3 MsType 类型槽 |
| `syntax.md` | §2.4 逻辑运算符（and/or/not，Python 语义） |
| `vm.md` | §3.4 比较与逻辑（NOT/AND_JMP/OR_JMP）；§3.5 跳转；§9 opcode 命名映射 |

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
static MsValue boolRepr(struct MsVM* vm, MsValue v) {
  return MS_AS_BOOL(v) ? msNewStr(vm, "true", 4) : msNewStr(vm, "false", 5);
}

static MsValue boolEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void)vm;
  // bool == bool
  if (MS_IS_BOOL(b)) return MS_BOOL_VAL(MS_AS_BOOL(a) == MS_AS_BOOL(b));
  // bool == int（true==1, false==0，兼容 Python）
  if (MS_IS_INT(b))  return MS_BOOL_VAL((int64_t)MS_AS_BOOL(a) == MS_AS_INT(b));
  // bool == float（true==1.0, false==0.0，兼容 Python）
  if (MS_IS_FLOAT(b)) return MS_BOOL_VAL((double)(MS_AS_BOOL(a) ? 1 : 0) == MS_AS_FLOAT(b));
  return MS_BOOL_VAL(false);
}

static MsValue boolHash(struct MsVM* vm, MsValue v) {
  (void)vm;
  return MS_INT_VAL(MS_AS_BOOL(v) ? 1 : 0);
}

MsType msBoolType = {
  .name = "bool", .objSize = 0,  // 标量，不分配堆对象
  .tpRepr = boolRepr, .tpStr = boolRepr,
  .tpHash = boolHash, .tpEq  = boolEq,
};
```

### 2. nil 类型槽

```c
static MsValue nilRepr(struct MsVM* vm, MsValue v) { (void)v; return msNewStr(vm, "nil", 3); }
static MsValue nilEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void)vm; (void)a; return MS_BOOL_VAL(MS_IS_NIL(b));
}
static MsValue nilHash(struct MsVM* vm, MsValue v) { (void)vm; (void)v; return MS_INT_VAL(0); }

MsType msNilType = {
  .name = "nil", .objSize = 0,  // 单例，不分配堆对象
  .tpRepr = nilRepr, .tpStr = nilRepr,
  .tpHash = nilHash, .tpEq  = nilEq,
};
```

### 3. VM 指令

```c
case OP_NOT: {
  MsValue v = POP();
  PUSH(MS_BOOL_VAL(!msValueTruthy(vm, v)));
  DISPATCH();
}

// 跳转操作数一律 3 字节有符号 24 位 AX（vm.md §3 约定），符号扩展后得到偏移
// int32_t offset = (int32_t)(READ_AX() << 8) >> 8;

// 条件跳转（弹出栈顶）
case OP_JUMP_IF_FALSE: {
  int32_t offset = (int32_t)(READ_AX() << 8) >> 8;
  MsValue v = POP();
  if (!msValueTruthy(vm, v)) frame->ip += offset;
  DISPATCH();
}
case OP_JUMP_IF_TRUE: {
  int32_t offset = (int32_t)(READ_AX() << 8) >> 8;
  MsValue v = POP();
  if (msValueTruthy(vm, v)) frame->ip += offset;
  DISPATCH();
}
// 短路 and/or（不弹出栈顶，保留结果值）
case OP_AND_JMP: {
  int32_t offset = (int32_t)(READ_AX() << 8) >> 8;
  if (!msValueTruthy(vm, PEEK(0))) frame->ip += offset;
  DISPATCH();
}
case OP_OR_JMP: {
  int32_t offset = (int32_t)(READ_AX() << 8) >> 8;
  if (msValueTruthy(vm, PEEK(0))) frame->ip += offset;
  DISPATCH();
}
// 无条件跳转：正数前跳，负数回跳（循环回边复用此指令，无独立 OP_LOOP）
case OP_JUMP: {
  int32_t offset = (int32_t)(READ_AX() << 8) >> 8;
  frame->ip += offset;
  DISPATCH();
}
```

### 4. 真值规则（`msValueTruthy`）

```c
bool msValueTruthy(struct MsVM* vm, MsValue v) {
  switch (v.tag) {
  case MS_TAG_NIL:   return false;
  case MS_TAG_BOOL:  return MS_AS_BOOL(v);
  case MS_TAG_INT:   return MS_AS_INT(v) != 0;
  case MS_TAG_FLOAT: return MS_AS_FLOAT(v) != 0.0;  // nan 为真（IEEE 754：nan != 0.0）
  case MS_TAG_OBJ: {
    MsObject* obj = MS_AS_OBJ(v);
    MsType* tp = obj->type;
    // Python 语义顺序：先 __bool__，无则 __len__，再默认为真
    if (tp->tpBool) return MS_AS_BOOL(tp->tpBool(vm, v));
    if (tp->tpLen) {
      MsValue len = tp->tpLen(vm, v);
      return MS_AS_INT(len) != 0;
    }
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
- [ ] `1 == true` → true（相等性对称；需同步为 P4-T053 `intEq` 增加 `MS_IS_BOOL(b)` 分支：`MS_AS_INT(a) == (int64_t)MS_AS_BOOL(b)`，见风险与边界）。
- [ ] `not (0.0/0.0)`（nan）→ false（`nan` 为真值，与 CPython 一致，非假值）。
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

- **`and`/`or` 返回值**：编译器（T039）已用短路跳转模式（`OP_AND_JMP`/`OP_OR_JMP`），结果为最后被求值的操作数（不强转 bool），与 Python 语义一致：`1 and 2` → 2，`0 or "x"` → "x"。
- **`nan` 真值**：`type-system.md §2.3` 假值列表不含 `nan`，故 `nan` 按非零浮点处理为真值，与 CPython 一致（`bool(float('nan'))` 为 `True`）。
- **`bool == int`/`float` 相等性对称**：`boolEq` 支持 `true == 1`；但 P4-T053 已实现的 `intEq` 未处理 `MS_IS_BOOL(b)` 分支，需在实现本任务时同步为 `intEq` 补充对称分支（`MS_AS_INT(a) == (int64_t)MS_AS_BOOL(b)`），否则 `1 == true` 会与 `true == 1` 结果不一致。
- **`bool` 是 `int` 子类**（Python）：mslang v1 中 `bool` 独立类型（不继承 int），但 `true + 1` 可考虑允许（初版暂不支持，报 TypeError）。
