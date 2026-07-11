# P4-T056 比较指令 + is / in / not in

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现比较运算指令（`OP_EQ`/`OP_NE`/`OP_LT`/`OP_GT`/`OP_LE`/`OP_GE`）以及身份判断（`OP_IS`/`OP_IS_NOT`）和成员判断（`OP_IN`/`OP_NOT_IN`）。这些指令覆盖所有条件表达式的运行时语义。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T053 | `intEq`/`intLt` 类型槽 |
| P4-T054 | `floatEq`/`floatLt` |
| P4-T055 | `msValueTruthy`/`msValueEqual`（已在 `src/runtime/ms_value.c` 实现，本任务复用，不重新定义） |
| P4-T051 | 求值循环 |
| P4-T049 | `MsType` 需新增 `tpContains` 槽（`type-system.md §1.3`，`__contains__` 目前无对应字段） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §1.3 MsType 类型槽（`tpEq`/`tpLt`/`tpContains`/`tpIter`/`tpNext`）；§3.4 魔术方法（`__eq__`/`__lt__`/`__contains__`）；§4 迭代器协议 |
| `syntax.md` | §1.10 运算符；§2.3 比较运算符优先级（`==`/`!=`/`<`/`<=`/`>`/`>=`/`in`/`is`/`not in`/`is not`） |
| `vm.md` | §3.4 比较与逻辑；§9 opcode 命名映射 |
| `errors.md` | 异常层级（`TypeError`） |

---

## 待实现（C 文件）

```
src/vm/ms_vm.c    # OP_EQ/NE/LT/GT/LE/GE/IS/IS_NOT/IN/NOT_IN case
```

---

## 实现要点

### 1. 相等比较（`==` / `!=`）

`msValueEqual(MsValue a, MsValue b)` 已在 P4-T055 实现（`src/runtime/ms_value.c`，声明见 `include/mslang/ms_value.h`），覆盖 int/float/bool 互相跨类型比较与对象 `tpEq` 分派。本任务不重新定义该函数，只新增消费它的 opcode：

```c
case OP_EQ: {
  MsValue b = POP(), a = POP();
  PUSH(MS_BOOL_VAL(msValueEqual(a, b)));
  DISPATCH();
}
case OP_NE: {
  MsValue b = POP(), a = POP();
  PUSH(MS_BOOL_VAL(!msValueEqual(a, b)));
  DISPATCH();
}
```

### 2. 顺序比较（`<` / `>` / `<=` / `>=`）

`msValueLt` 是本任务在 `ms_vm.c` 内新增的文件内 helper（同 `BINARY_OP`/`UNARY_OP` 宏一样使用文件作用域的 `&gVM`，无需单独 `vm` 形参）：

```c
static MsValue msValueLt(MsValue a, MsValue b) {
  struct MsType* ta = msTypeOf(a);
  if (!ta->tpLt) { return MS_ERROR_VALUE; }  // TypeError
  return ta->tpLt(&gVM, a, b);
}

case OP_LT: {
  MsValue b = POP(), a = POP();
  MsValue r = msValueLt(a, b);
  if (MS_IS_ERROR(r)) { return r; }
  PUSH(r);
  DISPATCH();
}
case OP_GT: {
  MsValue b = POP(), a = POP();
  MsValue r = msValueLt(b, a);   // a > b ≡ b < a
  if (MS_IS_ERROR(r)) { return r; }
  PUSH(r);
  DISPATCH();
}
case OP_LE: {
  MsValue b = POP(), a = POP();
  // a <= b ≡ not (b < a)
  MsValue r = msValueLt(b, a);
  if (MS_IS_ERROR(r)) { return r; }
  PUSH(MS_BOOL_VAL(!msValueTruthy(r)));
  DISPATCH();
}
case OP_GE: {
  MsValue b = POP(), a = POP();
  // a >= b ≡ not (a < b)
  MsValue r = msValueLt(a, b);
  if (MS_IS_ERROR(r)) { return r; }
  PUSH(MS_BOOL_VAL(!msValueTruthy(r)));
  DISPATCH();
}
```

### 3. 身份比较（`is` / `is not`）

```c
// is 检查对象身份（引用相等），不调用 __eq__
static bool msValueIs(MsValue a, MsValue b) {
  if (a.tag != b.tag) { return false; }
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
  // 栈自底向上：s[1]=item, s[0]=container（编译器按 "item in container" 顺序压栈）；
  // 先 POP 得 container（栈顶），再 POP 得 item
  MsValue container = POP(), item = POP();
  // 不可 in 的类型（无 tpContains）留给 msContains 内部返回 MS_ERROR_VALUE，
  // 此处不重复判定（避免与 msContains 的判定逻辑产生分歧）
  MsValue r = msContains(container, item);
  if (MS_IS_ERROR(r)) { return r; }
  PUSH(r);
  DISPATCH();
}
case OP_NOT_IN: {
  MsValue container = POP(), item = POP();
  MsValue r = msContains(container, item);
  if (MS_IS_ERROR(r)) { return r; }
  PUSH(MS_BOOL_VAL(!msValueTruthy(r)));
  DISPATCH();
}
```

```c
// msContains：调用容器类型自身的 tpContains 槽（T059 list / T060 map /
// T062 set 各自实现）。不提供通用的「线性扫描 tpIter/tpNext」fallback：
// StopIteration 的哨兵表示由 T065（GET_ITER/FOR_ITER 协议）敲定，本任务
// 阶段尚未定义，臆造哨兵值（如误用 MS_IS_NIL 判定结束）会导致
// `nil in [1, nil, 3]` 之类含 nil 元素的容器被误判为提前结束。
// 因此每种容器类型均须直接实现 tpContains（而非依赖此处的通用 fallback）。
static MsValue msContains(MsValue container, MsValue item) {
  struct MsType* tc = msTypeOf(container);
  if (tc->tpContains) { return tc->tpContains(&gVM, container, item); }
  return MS_ERROR_VALUE;  // TypeError: not iterable / no __contains__
}
```

---

## 验收标准（checklist）

- [ ] `1 == 1` → true；`1 == 2` → false。
- [ ] `1 == 1.0` → true（跨类型数值相等）。
- [ ] `2 != 1` → true；`1 != 1` → false。
- [ ] `1 < 2` → true；`2 < 1` → false。
- [ ] `1 <= 1` → true；`2 >= 3` → false。
- [ ] `"a" < "b"` → true（T057 str 实现后）。
- [ ] `nil is nil` → true。
- [ ] `nil is not 1` → true。
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
// 比较链（mslang 不做 Python 式链式比较；a < b < c 按左结合求值为
// (a < b) < c，需用 and 拆开表达真正的区间判断）
score := 5
print(score > 0 and score < 10)  // true

// is 身份比较
listA := [1, 2, 3]
aliasA := listA       // aliasA 是 listA 的别名（相同对象）
listB := [1, 2, 3]     // 不同对象，内容相同
print(listA is aliasA)  // true
print(listA is listB)   // false
print(listA == listB)   // true

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
- **`tpContains` 槽**：本任务需先为 `MsType`（type-system.md §1.3）新增 `tpContains` 字段（`__contains__` 此前无对应 C 槽）；初版在各类型构造中默认为 NULL，T059（list）、T060（map）、T062（set）中各自填充。本任务不提供通用的「线性扫描 tpIter/tpNext」fallback（该路径依赖 T065 才能敲定的 StopIteration 表示方式），故无 `tpContains` 的类型在其对应容器任务完成前 `in` 会返回 TypeError。
- **str 比较**：字节序比较（按 UTF-8 字节），不做 Unicode 归一化（v1 简化）。
