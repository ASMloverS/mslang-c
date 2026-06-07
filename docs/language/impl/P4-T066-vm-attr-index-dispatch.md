# P4-T066 属性 / 下标指令分派（类型槽）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `OP_GET_ATTR`/`OP_SET_ATTR`/`OP_DEL_ATTR`、`OP_GET_INDEX`/`OP_SET_INDEX`/`OP_DEL_INDEX` 六条属性/下标指令，通过 `MsType` 类型槽（`tpGetattr`/`tpSetattr`/`tpGetitem`/`tpSetitem`/`tpDelitem`）分派到各类型的具体实现。这是将所有核心类型统一接入属性/下标访问的关键指令。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 ~ T062 | 各类型已实现 `tpGetattr`/`tpGetitem`（或等效方法） |
| P4-T065 | `MsSliceObj`（下标可能是 slice） |
| P4-T051 | 求值循环 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §5 属性与下标指令 |
| `type-system.md` | §3 MsType 类型槽（tp_getattr / tp_getitem） |

---

## 待实现（C 文件）

```
src/vm/ms_vm.c    # OP_GET/SET/DEL_ATTR、OP_GET/SET/DEL_INDEX case
```

---

## 实现要点

### 1. `OP_GET_ATTR` / `OP_SET_ATTR` / `OP_DEL_ATTR`

```c
// OP_GET_ATTR [2B: nameIdx]
// 栈：[obj] → [attr_val]
case OP_GET_ATTR: {
  uint16_t nameIdx = READ_U16();
  MsValue obj  = POP();
  MsValue name = frame->chunk->consts[nameIdx];  // MsStr

  MsType* tp = msTypeOf(obj);
  MsValue result;

  if (tp->tpGetattr) {
    result = tp->tpGetattr(obj, name);
  } else {
    // 通用方法查找（从 methods 字典）
    result = msTypeLookupMethod(tp, name);
    if (!MS_IS_NIL(result)) {
      // 包装为绑定方法（T073 实现）
      result = msNewBoundMethod(result, obj);
    }
  }

  if (MS_IS_NIL(result)) {
    return msAttributeError(t, tp->name, MS_AS_OBJ(name) ? ((MsStrObj*)MS_AS_OBJ(name))->data : "?");
  }
  if (MS_IS_ERROR(result)) return result;
  PUSH(result);
  DISPATCH();
}

// OP_SET_ATTR [2B: nameIdx]
// 栈：[obj, val]（不弹出 obj？初版弹出两个）
case OP_SET_ATTR: {
  uint16_t nameIdx = READ_U16();
  MsValue val  = POP();
  MsValue obj  = POP();
  MsValue name = frame->chunk->consts[nameIdx];
  MsType* tp = msTypeOf(obj);
  if (!tp->tpSetattr) return msAttributeError(t, tp->name, "readonly");
  MsValue args[2] = {name, val};
  MsValue r = tp->tpSetattr(obj, args, 2);
  if (MS_IS_ERROR(r)) return r;
  DISPATCH();
}

// OP_DEL_ATTR [2B: nameIdx]
case OP_DEL_ATTR: {
  uint16_t nameIdx = READ_U16();
  MsValue obj  = POP();
  MsValue name = frame->chunk->consts[nameIdx];
  MsType* tp = msTypeOf(obj);
  if (!tp->tpDelattr) return msAttributeError(t, tp->name, "cannot delete");
  MsValue r = tp->tpDelattr(obj, name);
  if (MS_IS_ERROR(r)) return r;
  DISPATCH();
}
```

### 2. `OP_GET_INDEX` / `OP_SET_INDEX` / `OP_DEL_INDEX`

```c
// OP_GET_INDEX
// 栈：[obj, key] → [val]
case OP_GET_INDEX: {
  MsValue key = POP();
  MsValue obj = POP();
  MsType* tp  = msTypeOf(obj);

  // 切片检查
  if (MS_IS_OBJ(key) && MS_AS_OBJ(key)->type == &msSliceType) {
    if (!tp->tpGetslice) return msTypeError(t, "not subscriptable with slice");
    MsSliceObj* sl = (MsSliceObj*)MS_AS_OBJ(key);
    MsValue r = tp->tpGetslice(obj, sl);
    if (MS_IS_ERROR(r)) return r;
    PUSH(r);
    DISPATCH();
  }

  if (!tp->tpGetitem) return msTypeError(t, "not subscriptable");
  MsValue r = tp->tpGetitem(obj, key);
  if (MS_IS_ERROR(r)) return r;
  PUSH(r);
  DISPATCH();
}

// OP_SET_INDEX
// 栈：[obj, key, val]
case OP_SET_INDEX: {
  MsValue val = POP(), key = POP(), obj = POP();
  MsType* tp  = msTypeOf(obj);
  if (!tp->tpSetitem) return msTypeError(t, "does not support item assignment");
  MsValue args[2] = {key, val};
  MsValue r = tp->tpSetitem(obj, args, 2);
  if (MS_IS_ERROR(r)) return r;
  DISPATCH();
}

// OP_DEL_INDEX
// 栈：[obj, key]
case OP_DEL_INDEX: {
  MsValue key = POP(), obj = POP();
  MsType* tp  = msTypeOf(obj);
  if (!tp->tpDelitem) return msTypeError(t, "does not support item deletion");
  MsValue r = tp->tpDelitem(obj, key);
  if (MS_IS_ERROR(r)) return r;
  DISPATCH();
}
```

### 3. 方法查找（T073 前的简化实现）

```c
// 在 MsType 的 methods 字典（MsMap*）中查找名称
// T073 之前，methods 为 NULL，此处总返回 NIL
MsValue msTypeLookupMethod(MsType* tp, MsValue name) {
  if (!tp->methods) return MS_NIL_VAL;
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
- [ ] `"hello"[0]` → "h"（str index）。
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
- **切片与非切片**：`OP_GET_INDEX` 在运行时检查 key 类型（是否为 `MsSliceObj`）来决定走 `tpGetitem` 还是 `tpGetslice`；编译器总是生成 `OP_BUILD_SLICE` + `OP_GET_INDEX`，VM 动态分派。
