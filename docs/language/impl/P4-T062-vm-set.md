# P4-T062 set 类型（集合 / 关系运算）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `set` 运行时类型（`MsSetObj`）：可变无序集合，元素必须可哈希。独立结构，采用与 `MsMapObj` 相同的开放寻址/墓碑约定（只存元素，无 value）。支持成员检查、集合运算（`|`/`&`/`-`/`^`）与关系运算（`<=`/`<`/`>=`/`>`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T060 | `MsMapObj` 哈希表（set 复用其结构与开放寻址/墓碑约定）|
| P4-T050 | `msGCAlloc` |

> **前置 VM 派发扩展**（本任务范围内，非独立任务号）：`MsType` 目前无按位运算槽，`OP_BOR/BAND/BXOR` 仅支持 int（`ms_vm.c` `BITWISE_OP` 宏）；`OP_LE/GE/GT` 目前全部由 `tpLt` 取反/交换派生，从不调用已存在但从未被使用的 `tpLe`/`tpGe`/`tpGt` 槽。set 的 `| & ^` 与 `<= < >= >` 要求先扩展这两处 VM 派发逻辑，详见「实现要点 §0」。

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §1.3 MsType（新增 tpBitor/tpBitand/tpBitxor 槽）、§2.10 set |

---

## 待实现（C 文件）

```
src/runtime/ms_set.c        # MsSetObj + 类型槽 + 方法
include/mslang/ms_set.h     # msNewSet / msSetAdd / msSetHas / msSetRemove / msSetDiscard
include/mslang/ms_object.h  # 改：MsType 新增 tpBitor/tpBitand/tpBitxor 槽
src/vm/ms_vm.c               # 改：BITWISE_OP 非 int 操作数回退类型槽；OP_LE/GE/GT 优先调用 tpLe/tpGe/tpGt
src/compiler/ms_compiler.c   # 改：新增 compileSet，MS_ND_SET → OP_BUILD_SET（OP_BUILD_SET 与反汇编表已在 opcode 枚举/ms_disasm.c 预留）
```

---

## 实现要点

### 0. VM 派发扩展（前置，改 `ms_object.h` / `ms_vm.c`）

**按位运算 `| & ^`**：`MsType` 新增三个槽（`type-system.md §1.3`）：

```c
MsBinaryFn tpBitor;   // __or__
MsBinaryFn tpBitand;  // __and__
MsBinaryFn tpBitxor;  // __xor__
```

`BITWISE_OP` 宏改为非 int 操作数时回退到类型槽（模式同 `BINARY_OP`）：

```c
#define BITWISE_OP(op, slot)                                                    \
  do {                                                                          \
    MsValue b = POP(), a = POP();                                               \
    if (MS_IS_INT(a) && MS_IS_INT(b)) {                                         \
      PUSH(MS_INT_VAL(MS_AS_INT(a) op MS_AS_INT(b)));                           \
    } else {                                                                    \
      struct MsType* ta = msTypeOf(a);                                          \
      MsValue r = ta->slot ? ta->slot(&gVM, a, b) : msNotImplemented(&gVM, a, b); \
      if (MS_IS_ERROR(r)) { return r; }                                         \
      PUSH(r);                                                                  \
    }                                                                           \
  } while (0)
// 调用处：BITWISE_OP(|, tpBitor) / BITWISE_OP(&, tpBitand) / BITWISE_OP(^, tpBitxor)
```

**富比较 `<= < >= >`**：`tpLe`/`tpGe`/`tpGt` 槽在 `MsType` 中早已定义（`type-system.md §1.3`）但从未被调用——`OP_LE/GE/GT` 目前全部由 `tpLt` 取反/交换派生，对集合的偏序关系（非全序）给出错误结果。改为优先调用自身槽，缺失时回退到现有的 `tpLt` 派生（保持 int/float/str 等既有类型行为不变）：

```c
// a <= b：优先 tpLe（富比较，如子集）；否则回退 not(b < a)（全序类型）。
static MsValue msValueLe(MsValue a, MsValue b) {
  struct MsType* ta = msTypeOf(a);
  if (ta->tpLe) { return ta->tpLe(&gVM, a, b); }
  MsValue lt = msValueLt(b, a);
  if (MS_IS_ERROR(lt)) { return lt; }
  return MS_BOOL_VAL(!msValueTruthy(lt));
}

// a >= b：优先 tpGe；否则回退 not(a < b)。
static MsValue msValueGe(MsValue a, MsValue b) {
  struct MsType* ta = msTypeOf(a);
  if (ta->tpGe) { return ta->tpGe(&gVM, a, b); }
  MsValue lt = msValueLt(a, b);
  if (MS_IS_ERROR(lt)) { return lt; }
  return MS_BOOL_VAL(!msValueTruthy(lt));
}

// a > b：优先 tpGt；否则回退 b < a。
static MsValue msValueGt(MsValue a, MsValue b) {
  struct MsType* ta = msTypeOf(a);
  if (ta->tpGt) { return ta->tpGt(&gVM, a, b); }
  return msValueLt(b, a);
}
```

`OP_LE`/`OP_GE`/`OP_GT` 分别改为直接 `PUSH` 上述函数的返回值（不再经过 `COMPARE_OP` 的 `negate` 二次取反）；`OP_LT` 不变。

### 1. MsSetObj 结构

对齐 `type-system.md §2.10` 与既有 `MsMapObj`（`ms_map.h`）实现约定，两者共用同一开放寻址/墓碑策略：

```c
struct MsSetEntry {
  MsValue  item;      // 元素
  uint32_t hash;      // occupied 时为缓存 hash；!occupied 时复用为 tombstone 标记
  bool     occupied;
};

struct MsSetObj {
  struct MsObject     head;
  uint32_t            len;         // 元素数
  uint32_t            cap;         // 槽位总数，必须为 2 的幂
  uint32_t            tombstones;  // 已删除槽位数（驱动装载因子检查）
  struct MsSetEntry*  entries;
};
```

### 2. 核心操作

`initCap` 仅为容量提示，`msNewSet` 内部向上取整到 2 的幂（最小 8，与 `msNewMap` 一致）。可能因元素不可哈希 / 键不存在报错的操作返回 `MsValue`（`MS_ERROR_VALUE` 表示 TypeError/KeyError，同 `msMapSet`/`msMapDel` 约定）：

```c
MsValue msNewSet(uint32_t initCap);

MsValue msSetAdd(struct MsVM* vm, MsValue setVal, MsValue v);      // 添加（幂等）；元素不可哈希 → TypeError
MsValue msSetHas(struct MsVM* vm, MsValue setVal, MsValue v);      // 成员检查（返回 MS_BOOL_VAL；不可哈希 → TypeError）
MsValue msSetRemove(struct MsVM* vm, MsValue setVal, MsValue v);   // 移除（不存在 → KeyError）
MsValue msSetDiscard(struct MsVM* vm, MsValue setVal, MsValue v);  // 移除（不存在 → 静默忽略）
```

### 3. 集合运算

```c
// a | b → union
MsValue msSetUnion(struct MsVM* vm, MsValue a, MsValue b);

// a & b → intersection
MsValue msSetIntersect(struct MsVM* vm, MsValue a, MsValue b);

// a - b → difference
MsValue msSetDiff(struct MsVM* vm, MsValue a, MsValue b);

// a ^ b → symmetric difference
MsValue msSetSymDiff(struct MsVM* vm, MsValue a, MsValue b);

// a <= b → subset
bool    msSetIsSubset(struct MsSetObj* a, struct MsSetObj* b);

// a < b → proper subset
bool    msSetIsProperSubset(struct MsSetObj* a, struct MsSetObj* b);

// a >= b → superset
bool    msSetIsSuperset(struct MsSetObj* a, struct MsSetObj* b);

// a > b → proper superset
bool    msSetIsProperSuperset(struct MsSetObj* a, struct MsSetObj* b);
```

### 4. VM 指令

`OP_BUILD_SET` 使用 1 字节 FMT_A 操作数（同 `OP_BUILD_LIST`/`OP_BUILD_MAP`，已在 opcode 枚举与 `ms_disasm.c` 预留）。元素可能不可哈希（TypeError），故采用与 `OP_BUILD_MAP` 相同的 `msGCPushRoot` 保护 + 错误传播模式：

```c
// OP_BUILD_SET [1B FMT_A: count]
case OP_BUILD_SET: {
  uint8_t count = READ_BYTE();
  MsValue* items = t->sp - count;
  MsValue setVal = msNewSet(count);
  msGCPushRoot(setVal);
  for (uint8_t i = 0; i < count; i++) {
    MsValue r = msSetAdd(&gVM, setVal, items[i]);
    if (MS_IS_ERROR(r)) {
      msGCPopRoot();
      return r;  // TypeError: 元素不可哈希
    }
  }
  msGCPopRoot();
  t->sp = items;
  PUSH(setVal);
  DISPATCH();
}
```

### 5. 类型槽

槽函数签名须匹配 `MsBinaryFn = MsValue (*)(struct MsVM* vm, MsValue a, MsValue b)`（`ms_object.h`），均带 `vm` 首参：

```c
// set | set → union
static MsValue setOr(struct MsVM* vm, MsValue a, MsValue b) {
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
  return msSetUnion(vm, a, b);
}

// set & set → intersection
static MsValue setAnd(struct MsVM* vm, MsValue a, MsValue b) {
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
  return msSetIntersect(vm, a, b);
}

// set - set → difference
static MsValue setSub(struct MsVM* vm, MsValue a, MsValue b) {
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
  return msSetDiff(vm, a, b);
}

// set ^ set → symmetric difference
static MsValue setXor(struct MsVM* vm, MsValue a, MsValue b) {
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
  return msSetSymDiff(vm, a, b);
}

// set <= set → subset；set < set → proper subset
// set >= set → superset；set > set → proper superset
static MsValue setLe(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
  return MS_BOOL_VAL(msSetIsSubset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

static MsValue setLt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
  return MS_BOOL_VAL(msSetIsProperSubset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

static MsValue setGe(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
  return MS_BOOL_VAL(msSetIsSuperset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

static MsValue setGt(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
  return MS_BOOL_VAL(msSetIsProperSuperset((struct MsSetObj*) MS_AS_OBJ(a), (struct MsSetObj*) MS_AS_OBJ(b)));
}

// GC 遍历：访问每个 occupied 槽的元素地址（供未来移动式 GC 原地改写，同 mapTraverse）。
static void setTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsSetObj* s = (struct MsSetObj*) obj;
  for (uint32_t i = 0; i < s->cap; i++) {
    struct MsSetEntry* e = &s->entries[i];
    if (e->occupied) {
      visit(&e->item, ctx);
    }
  }
}

static void setDestroy(struct MsObject* obj) {
  msFree(((struct MsSetObj*) obj)->entries);
}

struct MsType msSetType = {
  .name       = "set",
  .objSize    = sizeof(struct MsSetObj),
  .traverse   = setTraverse,
  .destroy    = setDestroy,
  .tpLen      = setLen,
  .tpEq       = setEq,
  .tpContains = setContains,
  .tpIter     = setIter,
  .tpBitor    = setOr,
  .tpSub      = setSub,
  .tpBitand   = setAnd,
  .tpBitxor   = setXor,
  .tpLe       = setLe,
  .tpLt       = setLt,   // 真子集
  .tpGe       = setGe,
  .tpGt       = setGt,   // 真超集
};
```

### 6. set 方法

| 方法 | 说明 |
|---|---|
| `add(v)` | 添加元素 |
| `remove(v)` | 移除（不存在 → KeyError） |
| `discard(v)` | 移除（不存在忽略） |
| `pop()` | 随机移除并返回一个元素 |
| `clear()` | 清空 |
| `copy()` | 浅拷贝 |
| `union(other)` | 等同 `|` |
| `intersection(other)` | 等同 `&` |
| `difference(other)` | 等同 `-` |
| `symmetricDifference(other)` | 等同 `^` |
| `isSubset(other)` | 等同 `<=` |
| `isSuperset(other)` | 等同 `>=` |
| `isDisjoint(other)` | 无公共元素 |
| `update(other)` | 原地 `|=` |
| `intersectionUpdate(other)` | 原地 `&=` |
| `differenceUpdate(other)` | 原地 `-=` |
| `symmetricDifferenceUpdate(other)` | 原地 `^=` |

---

## 验收标准（checklist）

- [ ] `{1, 2, 3}` → 长度为 3 的 set。
- [ ] `1 in {1, 2, 3}` → true；`5 in {1, 2, 3}` → false。
- [ ] `{1,2} | {2,3}` → `{1,2,3}`。
- [ ] `{1,2,3} & {2,3,4}` → `{2,3}`。
- [ ] `{1,2,3} - {2}` → `{1,3}`。
- [ ] `{1,2} <= {1,2,3}` → true（子集）。
- [ ] `{1,2} < {1,2}` → false（非真子集）。
- [ ] 重复元素自动去重：`{1,1,2,2}` → len=2。
- [ ] 不可哈希类型加入 set → TypeError。
- [ ] GC：set 中元素正确被 mark。

---

## 测试用例（C 单测）

### `tests/vm/test_set.c`

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

static void testSetBuild(void) {
  MsValue v = run("len({1, 2, 2, 3})");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 3, "dedup len=3");
}

static void testSetOps(void) {
  MsValue v = run("len({1,2} | {2,3})");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 3, "union len=3");
}

int main(void) {
  MS_RUN(testSetBuild);
  MS_RUN(testSetOps);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
// 去重
nums := [1, 2, 2, 3, 3, 3]
unique := set(nums)
print(len(unique))  // 3

// 集合运算
a := {1, 2, 3, 4}
b := {3, 4, 5, 6}
print(a | b)     // {1, 2, 3, 4, 5, 6}
print(a & b)     // {3, 4}
print(a - b)     // {1, 2}
print(a ^ b)     // {1, 2, 5, 6}

// 关系判断
print({1,2} <= {1,2,3})  // true（子集）
print({1,2}.isDisjoint({3,4}))  // true

// 成员检查（O(1)）
large := set(range(1_000_000))
print(999999 in large)   // true
```

---

## Benchmark

```ms
// benchmarks/bench_set.ms
n := 1_000_000
s := set(range(n))
found := 0
for i in range(n) {
    if i in s {
        found += 1
    }
}
print(found)  // 1000000
// 目标：> 10M membership checks/sec
```

---

## 风险与边界

- **集合运算返回新 set**（不修改原集合）；原地版本为 `update`/`intersectionUpdate`/`differenceUpdate`/`symmetricDifferenceUpdate`（方法形式，camelCase，`ms-style.md §5.2`）。
- **迭代顺序**：set 是无序的，迭代顺序由内部哈希桶顺序决定（不保证任何特定顺序）。
- **`set` vs `frozenset`（T063）**：`set` 可变（不可 hash），`frozenset` 不可变（可 hash，可作 set/map 键）。`{1,2}` 字面量创建 `set`，`frozenset([1,2])` 创建 `frozenset`。
- **`.ms` 示例/Benchmark 中的 `set(iterable)`/`range()`/`print()` 属前瞻性用法**：这些内置函数在 P8（T096–T105）才实现，本任务的验收仅覆盖 `{...}` 字面量语法与 C 单测（同 T059/T060/T061 既有约定），示例代码在 P8 完成前不可直接运行。
