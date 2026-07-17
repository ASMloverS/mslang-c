# P4-T066 属性 / 下标指令分派（类型槽）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `OP_GET_ATTR`/`OP_SET_ATTR`/`OP_DEL_ATTR`、`OP_GET_ITEM`/`OP_SET_ITEM`/`OP_DEL_ITEM` 六条属性/下标指令，通过 `MsType` 类型槽（`tpGetattr`/`tpSetattr`/`tpDelattr`/`tpGetitem`/`tpSetitem`/`tpDelitem`）分派到各类型的具体实现。其中 `tpGetitem`/`tpSetitem` 已随 T057–T065 落地并接入 `src/vm/ms_vm.c`；`tpGetattr`/`tpSetattr`/`tpDelattr`/`tpDelitem` 四个类型槽由本任务新增。这是将所有核心类型统一接入属性/下标访问的关键指令。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 ~ T062 | 各类型已实现 `tpGetitem`/`tpSetitem`（`OP_GET_ITEM`/`OP_SET_ITEM` 已随之接入求值循环）；`tpGetattr` 由本任务新增并接入各内置类型 |
| P4-T065 | `MsSliceObj`（下标可能是 slice） |
| P4-T051 | 求值循环 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3.7 属性与下标指令、§9 opcode 命名映射 |
| `type-system.md` | §1.3 MsType 类型槽（`tpGetattr` / `tpSetattr` / `tpDelattr` / `tpGetitem` / `tpSetitem` / `tpDelitem`） |
| `errors.md` | §1 异常层次结构（`AttributeError`/`TypeError`/`IndexError`/`KeyError`）、§5 VM 异常传播机制 |

---

## 待实现（C 文件）

```
src/vm/ms_vm.c    # OP_GET/SET/DEL_ATTR、OP_DEL_ITEM case（OP_GET/SET_ITEM 已在 T057-T065 落地）
```

---

## 实现要点

### 1. `OP_GET_ATTR` / `OP_SET_ATTR` / `OP_DEL_ATTR`

```c
// OP_GET_ATTR [AX: nameIdx]
// 栈：[obj] → [attr_val]
case OP_GET_ATTR: {
  uint32_t nameIdx = READ_AX();
  MsValue obj  = POP();
  MsValue name = frame->chunk->constants[nameIdx];  // MsStr

  struct MsType* tp = msTypeOf(obj);
  MsValue result;

  if (tp->tpGetattr) {
    result = tp->tpGetattr(&gVM, obj, name);
  } else {
    // 通用方法查找（从 methods 字典）
    result = msTypeLookupMethod(tp, name);
    if (!MS_IS_NIL(result)) {
      // 包装为绑定方法（T073 实现）
      result = msNewBoundMethod(result, obj);
    }
  }

  if (MS_IS_NIL(result)) {
    return MS_ERROR_VALUE;  // AttributeError（T080 前占位，errors.md §1/§5）
  }
  if (MS_IS_ERROR(result)) {
    return result;
  }
  PUSH(result);
  DISPATCH();
}

// OP_SET_ATTR [AX: nameIdx]
// 栈：[val, obj]（compileAssign 先压 val 再压 obj，obj 在栈顶，见 ms_compiler.c）
case OP_SET_ATTR: {
  uint32_t nameIdx = READ_AX();
  MsValue obj  = POP();
  MsValue val  = POP();
  MsValue name = frame->chunk->constants[nameIdx];
  struct MsType* tp = msTypeOf(obj);
  if (!tp->tpSetattr) {
    return MS_ERROR_VALUE;  // AttributeError: readonly（T080 前占位）
  }
  MsValue r = tp->tpSetattr(&gVM, obj, name, val);
  if (MS_IS_ERROR(r)) {
    return r;
  }
  DISPATCH();
}

// OP_DEL_ATTR [AX: nameIdx]
// 栈：[obj]
case OP_DEL_ATTR: {
  uint32_t nameIdx = READ_AX();
  MsValue obj  = POP();
  MsValue name = frame->chunk->constants[nameIdx];
  struct MsType* tp = msTypeOf(obj);
  if (!tp->tpDelattr) {
    return MS_ERROR_VALUE;  // AttributeError: cannot delete（T080 前占位）
  }
  MsValue r = tp->tpDelattr(&gVM, obj, name);
  if (MS_IS_ERROR(r)) {
    return r;
  }
  DISPATCH();
}
```

### 2. `OP_GET_ITEM` / `OP_SET_ITEM` / `OP_DEL_ITEM`

> `OP_GET_ITEM`/`OP_SET_ITEM` 已在 T057–T065 期间随 `tpGetitem`/`tpSetitem` 实现并接入 `src/vm/ms_vm.c`（`BINARY_OP(tpGetitem)` 宏），此处列出以说明分派模式；`OP_DEL_ITEM` 为本任务新增。切片统一走 `tpGetitem`（key 为 `MsSliceObj`，编译器总是生成 `OP_BUILD_SLICE` + `OP_GET_ITEM`，见 `compileSliceExpr`），不设独立 `tpGetslice` 槽。

```c
// OP_GET_ITEM
// 栈：[obj, key] → [val]（已实现，见 src/vm/ms_vm.c）
case OP_GET_ITEM:
  BINARY_OP(tpGetitem);
  DISPATCH();

// OP_SET_ITEM
// 栈：[val, obj, key]（已实现，见 src/vm/ms_vm.c）
case OP_SET_ITEM: {
  MsValue key = POP(), obj = POP(), val = POP();
  struct MsType* tp = msTypeOf(obj);
  MsValue r = tp->tpSetitem ? tp->tpSetitem(&gVM, obj, key, val) : MS_ERROR_VALUE;
  if (MS_IS_ERROR(r)) {
    return r;
  }
  PUSH(r);
  DISPATCH();
}

// OP_DEL_ITEM
// 栈：[obj, key]
case OP_DEL_ITEM: {
  MsValue key = POP(), obj = POP();
  struct MsType* tp = msTypeOf(obj);
  if (!tp->tpDelitem) {
    return MS_ERROR_VALUE;  // TypeError: does not support item deletion（T080 前占位）
  }
  MsValue r = tp->tpDelitem(&gVM, obj, key);
  if (MS_IS_ERROR(r)) {
    return r;
  }
  DISPATCH();
}
```

### 3. 方法查找（T073 前的简化实现）

```c
// 在 MsType 的 methods 字典（MsMap*）中查找名称
// T073 之前，methods 为 NULL，此处总返回 NIL
MsValue msTypeLookupMethod(struct MsType* tp, MsValue name) {
  if (!tp->methods) {
    return MS_NIL_VAL;
  }
  return msMapGet(MS_OBJ_VAL((MsObject*)tp->methods), name);
}
```

内置类型的方法（list.append 等）在 T057–T062 中通过 `tpGetattr` 直接查找（不走 methods 字典），简化实现：

```c
// 例：str 的 tpGetattr
static MsValue strGetAttr(MsValue v, MsValue name) {
  MsStrObj* s = (MsStrObj*)MS_AS_OBJ(v);
  if (!MS_IS_OBJ(name)) return MS_NIL_VAL;
  MsStrObj* n = (MsStrObj*)MS_AS_OBJ(name);
  if (memcmp(n->data, "upper", 5) == 0)
    return msMakeBuiltinMethod(strMethodUpper, v);
  if (memcmp(n->data, "lower", 5) == 0)
    return msMakeBuiltinMethod(strMethodLower, v);
  // ... 更多方法
  return MS_NIL_VAL;
}
```

### 4. 实例属性访问（T072 前的占位）

```c
// MsInstanceObj（T072 实现）拥有 attrs（MsMap*）
// 在 T072 之前，GET_ATTR 只处理内置类型；
// 对用户定义类型的属性访问在 T072 后填充
```

---

## 验收标准（checklist）

- [ ] `[1,2,3][1]` → 2（list index）。
- [ ] `"hello"[0]` → 104（str index，返回字节值 int，见 `type-system.md §2.5`）。
- [ ] `{"a": 1}["a"]` → 1（map index）。
- [ ] `lst[0] = 99` → list 第 0 个元素被修改。
- [ ] `del m["a"]` → map 键被删除。
- [ ] `"hello".upper()` → "HELLO"（str 方法通过 GET_ATTR）。
- [ ] `[1,2,3].append(4)` → list 被修改（T059 后）。
- [ ] 越界下标 → IndexError（MS_ERROR_VALUE，T080 后完整报错）。
- [ ] `"hello"[1:3]` → "el"（切片，通过 MsSliceObj）。
- [ ] 不可订阅类型（int）的下标 → TypeError。

---

## 测试用例（C 单测）

### `tests/vm/test_attr_index.c`

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

static void testListGetIndex(void) {
  MsValue v = run("[10, 20, 30][2]");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 30, "list[2]=30");
}

static void testListSetIndex(void) {
  MsValue v = run("l := [1,2,3]\nl[0] = 99\nl[0]");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 99, "l[0]=99");
}

static void testStrMethod(void) {
  MsValue v = run("\"hello\".upper()");
  MsStrObj* s = (MsStrObj*)MS_AS_OBJ(v);
  MS_ASSERT_TRUE(memcmp(s->data, "HELLO", 5) == 0, "upper");
}

int main(void) {
  MS_RUN(testListGetIndex);
  MS_RUN(testListSetIndex);
  MS_RUN(testStrMethod);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
// 各类型属性/下标访问
lst := [1, 2, 3]
lst.append(4)
print(lst[-1])    // 4

m := {"x": 10}
m["y"] = 20
print(m.keys())   // ["x", "y"]

s := "hello world"
print(s.split(" "))  // ["hello", "world"]
print(s[6:])         // world

// 嵌套
data := {"users": [{"name": "alice"}, {"name": "bob"}]}
print(data["users"][1]["name"])   // bob
```

---

## Benchmark

N/A（属性/下标分派性能在整体 VM bench 中体现）。

---

## 风险与边界

- **方法调用 vs 属性访问**：`obj.method` 返回绑定方法对象，随后 `OP_CALL` 调用；T073 实现完整的绑定方法对象（`MsBoundMethodObj`）。T066 阶段 `GET_ATTR` 对内置方法返回可调用的 C 函数包装（`MsBuiltinMethod`）。
- **`tpGetattr` 的方法返回**：内置类型（list/str 等）在 `tpGetattr` 中手动检查名称字符串；用户定义类型在 T072/T073 通过 `methods` 字典查找。
- **切片与非切片统一走 `tpGetitem`**：编译器总是生成 `OP_BUILD_SLICE` + `OP_GET_ITEM`（`compileSliceExpr`，`src/compiler/ms_compiler.c`），key 是否为 `MsSliceObj` 由各类型自身的 `tpGetitem` 实现判断，`OP_GET_ITEM` 分派层不做特判，不设独立 `tpGetslice` 槽。
