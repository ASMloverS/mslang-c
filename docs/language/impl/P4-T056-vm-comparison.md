# P4-T056 比较指令 + is / in / not in

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现比较运算指令（`OP_EQ`/`OP_NEQ`/`OP_LT`/`OP_GT`/`OP_LE`/`OP_GE`）以及身份判断（`OP_IS`/`OP_IS_NOT`）和成员判断（`OP_IN`/`OP_NOT_IN`）。这些指令覆盖所有条件表达式的运行时语义。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T053 | `intEq`/`intLt` 类型槽 |
| P4-T054 | `floatEq`/`floatLt` |
| P4-T055 | `msValueTruthy` |
| P4-T051 | 求值循环 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §7 比较协议（tp_eq / tp_lt）|
| `syntax.md` | §2.2 比较运算符（is/in/not in）|

---

## 待实现（C 文件）

```
src/vm/ms_vm.c    # OP_EQ/NEQ/LT/GT/LE/GE/IS/IS_NOT/IN/NOT_IN case
```

---

## 实现要点

### 1. 相等比较（`==` / `!=`）

```c
// 全局 msValueEqual 实现
bool msValueEqual(MsValue a, MsValue b) {
  if (a.tag != b.tag) {
    // 跨类型：int vs float
    if (MS_IS_INT(a) && MS_IS_FLOAT(b))
      return (double)MS_AS_INT(a) == MS_AS_FLOAT(b);
    if (MS_IS_FLOAT(a) && MS_IS_INT(b))
      return MS_AS_FLOAT(a) == (double)MS_AS_INT(b);
    // bool vs int（true==1, false==0）
    if (MS_IS_BOOL(a) && MS_IS_INT(b))
      return (int64_t)MS_AS_BOOL(a) == MS_AS_INT(b);
    if (MS_IS_INT(a) && MS_IS_BOOL(b))
      return MS_AS_INT(a) == (int64_t)MS_AS_BOOL(b);
    return false;
  }
  switch (a.tag) {
  case MS_TAG_NIL:   return true;
  case MS_TAG_BOOL:  return MS_AS_BOOL(a) == MS_AS_BOOL(b);
  case MS_TAG_INT:   return MS_AS_INT(a) == MS_AS_INT(b);
  case MS_TAG_FLOAT: return MS_AS_FLOAT(a) == MS_AS_FLOAT(b);
  case MS_TAG_OBJ: {
    MsObject* oa = MS_AS_OBJ(a), *ob = MS_AS_OBJ(b);
    if (oa == ob) return true;  // 身份相等
    if (oa->type->tpEq) {
      MsValue r = oa->type->tpEq(a, b);
      return MS_IS_BOOL(r) && MS_AS_BOOL(r);
    }
    return false;  // 默认：身份比较
  }
  default: return false;
  }
}

case OP_EQ: {
  MsValue b = POP(), a = POP();
  PUSH(MS_BOOL_VAL(msValueEqual(a, b)));
  DISPATCH();
}
case OP_NEQ: {
  MsValue b = POP(), a = POP();
  PUSH(MS_BOOL_VAL(!msValueEqual(a, b)));
  DISPATCH();
}
```

### 2. 顺序比较（`<` / `>` / `<=` / `>=`）

```c
static MsValue msValueLt(MsValue a, MsValue b) {
  MsType* ta = msTypeOf(a);
  if (!ta->tpLt) return MS_ERROR_VALUE;  // TypeError
  return ta->tpLt(a, b);
}

case OP_LT: {
  MsValue b = POP(), a = POP();
  MsValue r = msValueLt(a, b);
  if (MS_IS_ERROR(r)) return msTypeError(t, "not comparable");
  PUSH(r);
  DISPATCH();
}
case OP_GT: {
  MsValue b = POP(), a = POP();
  MsValue r = msValueLt(b, a);   // a > b ≡ b < a
  if (MS_IS_ERROR(r)) return msTypeError(t, "not comparable");
  PUSH(r);
  DISPATCH();
}
case OP_LE: {
  MsValue b = POP(), a = POP();
  // a <= b ≡ not (b < a)
  MsValue r = msValueLt(b, a);
  if (MS_IS_ERROR(r)) return msTypeError(t, "not comparable");
  PUSH(MS_BOOL_VAL(!msValueTruthy(r)));
  DISPATCH();
}
case OP_GE: {
  MsValue b = POP(), a = POP();
  // a >= b ≡ not (a < b)
  MsValue r = msValueLt(a, b);
  if (MS_IS_ERROR(r)) return msTypeError(t, "not comparable");
  PUSH(MS_BOOL_VAL(!msValueTruthy(r)));
  DISPATCH();
}
```

### 3. 身份比较（`is` / `is not`）

```c
// is 检查对象身份（引用相等），不调用 __eq__
static bool msValueIs(MsValue a, MsValue b) {
  if (a.tag != b.tag) return false;
  switch (a.tag) {
  case MS_TAG_NIL:   return true;   // nil is nil
  case MS_TAG_BOOL:  return MS_AS_BOOL(a) == MS_AS_BOOL(b);
  case MS_TAG_INT:   return MS_AS_INT(a) == MS_AS_INT(b);   // 小整数缓存语义
  case MS_TAG_FLOAT: return MS_AS_FLOAT(a) == MS_AS_FLOAT(b);
  case MS_TAG_OBJ:   return MS_AS_OBJ(a) == MS_AS_OBJ(b);  // 指针相等
  default:           return false;
  }
}

case OP_IS: {
  MsValue b = POP(), a = POP();
  PUSH(MS_BOOL_VAL(msValueIs(a, b)));
  DISPATCH();
}
case OP_IS_NOT: {
  MsValue b = POP(), a = POP();
  PUSH(MS_BOOL_VAL(!msValueIs(a, b)));
  DISPATCH();
}
```

### 4. 成员判断（`in` / `not in`）

```c
case OP_IN: {
  // 栈：[container, item]（注意：编译器按 "item in container" 顺序压栈）
  MsValue container = POP(), item = POP();
  MsType* tc = msTypeOf(container);
  if (!tc->tpGetitem && !tc->tpIter) {
    return msTypeError(t, "not iterable");
  }
  // 使用 __contains__ 槽（若有）或线性扫描迭代器
  MsValue r = msContains(container, item);
  if (MS_IS_ERROR(r)) return r;
  PUSH(r);
  DISPATCH();
}
case OP_NOT_IN: {
  MsValue container = POP(), item = POP();
  MsValue r = msContains(container, item);
  if (MS_IS_ERROR(r)) return r;
  PUSH(MS_BOOL_VAL(!msValueTruthy(r)));
  DISPATCH();
}
```

```c
// msContains：优先调用 tpContains（T059 list / T060 map / T062 set 实现），
//             否则线性扫描迭代器
MsValue msContains(MsValue container, MsValue item) {
  MsType* tc = msTypeOf(container);
  if (tc->tpContains) return tc->tpContains(container, item);
  // 线性扫描
  MsValue iter = tc->tpIter ? tc->tpIter(container) : MS_ERROR_VALUE;
  if (MS_IS_ERROR(iter)) return MS_ERROR_VALUE;
  MsType* ti = msTypeOf(iter);
  for (;;) {
    MsValue v = ti->tpNext(iter);
    if (MS_IS_NIL(v)) break;  // StopIteration 用 NIL 表示（T065）
    if (msValueEqual(v, item)) return MS_BOOL_VAL(true);
  }
  return MS_BOOL_VAL(false);
}
```

---

## 验收标准（checklist）

- [ ] `1 == 1` → true；`1 == 2` → false。
- [ ] `1 == 1.0` → true（跨类型数值相等）。
- [ ] `1 < 2` → true；`2 < 1` → false。
- [ ] `"a" < "b"` → true（T057 str 实现后）。
- [ ] `nil is nil` → true。
- [ ] `[1,2] is [1,2]` → false（两个不同对象）。
- [ ] `2 in [1, 2, 3]` → true（T059 后）。
- [ ] `5 not in [1, 2, 3]` → true（T059 后）。
- [ ] 不可比类型（如 nil < 1）→ TypeError（MS_ERROR_VALUE，T080 后完整报错）。

---

## 测试用例（C 单测）

### `tests/vm/test_comparison.c`

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

static void testEq(void) {
  MsValue v = run("1 == 1.0");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "1==1.0");
  v = run("nil == nil");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "nil==nil");
  v = run("nil == false");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "nil!=false");
}

static void testIs(void) {
  MsValue v = run("nil is nil");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "nil is nil");
}

int main(void) {
  MS_RUN(testEq);
  MS_RUN(testIs);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
// 比较链（mslang 不支持 a < b < c 链式比较，需拆开）
x := 5
print(x > 0 and x < 10)  // true

// is 身份比较
a := [1, 2, 3]
b := a        // b 是 a 的别名（相同对象）
c := [1, 2, 3]  // 不同对象，内容相同
print(a is b)   // true
print(a is c)   // false
print(a == c)   // true

// in 成员判断
print(2 in [1, 2, 3])       // true
print("key" in {"key": 1})  // true（map key）
print(4 not in [1, 2, 3])   // true
```

---

## Benchmark

N/A（比较指令成本在整体 VM bench 中体现）。

---

## 风险与边界

- **比较协议扩展**：`tpLt` 只定义 `<`；`>` 由 VM 反转（`b.tpLt(b, a)`），`<=`/`>=` 同理。若 a 和 b 类型不同且 a 不知如何与 b 比较（返回 `MS_ERROR_VALUE`），VM 尝试 b 的反向槽（反射协议），初版跳过此步骤（直接报 TypeError）。
- **`tpContains` 槽**：初版在 `MsType` 中暂定义为 NULL；T059（list）、T060（map/set）中填充。
- **str 比较**：字节序比较（按 UTF-8 字节），不做 Unicode 归一化（v1 简化）。
