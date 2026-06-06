# P4-T062 set 类型（集合 / 关系运算）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `set` 运行时类型（`MsSetObj`）：可变无序集合，元素必须可哈希。底层复用 map 的开放寻址哈希表（只存键，值为 DUMMY）。支持成员检查、集合关系运算（`|`/`&`/`-`/`^`/`<=`/`>=`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T060 | `MsMapObj` 哈希表（set 复用其结构）|
| P4-T050 | `msGCAlloc` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §13 set 类型 |

---

## 待实现（C 文件）

```
src/runtime/ms_set.c      # MsSetObj + 类型槽 + 方法
include/mslang/ms_set.h   # msNewSet / msSetAdd / msSetHas / msSetRemove
```

---

## 实现要点

### 1. MsSetObj 结构

```c
// set 底层复用 MsMapObj，将 MsMapEntry.val 置为 MS_BOOL_VAL(true)（DUMMY）
// 或使用独立的哈希表结构（只存 key + hash）
typedef struct MsSetEntry {
    MsValue  key;    // MS_TAG_NIL=空, MS_TAG_ERROR=tombstone
    uint32_t hash;
} MsSetEntry;

typedef struct MsSetObj {
    MsObject  header;
    uint32_t  count;
    uint32_t  cap;
    uint32_t  tombstones;
    MsSetEntry* entries;
} MsSetObj;
```

### 2. 核心操作

```c
MsValue msNewSet(uint32_t initCap);

void    msSetAdd(MsSetObj* s, MsValue v);      // 添加（幂等）
bool    msSetHas(MsSetObj* s, MsValue v);      // 成员检查
void    msSetRemove(MsSetObj* s, MsValue v);   // 移除（不存在 → KeyError）
void    msSetDiscard(MsSetObj* s, MsValue v);  // 移除（不存在 → 忽略）
```

### 3. 集合运算

```c
// a | b → union
MsValue msSetUnion(MsValue a, MsValue b);

// a & b → intersection
MsValue msSetIntersect(MsValue a, MsValue b);

// a - b → difference
MsValue msSetDiff(MsValue a, MsValue b);

// a ^ b → symmetric difference
MsValue msSetSymDiff(MsValue a, MsValue b);

// a <= b → subset
bool    msSetIsSubset(MsSetObj* a, MsSetObj* b);

// a < b → proper subset
bool    msSetIsProperSubset(MsSetObj* a, MsSetObj* b);
```

### 4. VM 指令

```c
// OP_BUILD_SET [2B: count]
case OP_BUILD_SET: {
    uint16_t count = READ_U16();
    MsValue setVal = msNewSet(count ? count * 2 : 4);
    MsSetObj* s = (MsSetObj*)MS_AS_OBJ(setVal);
    t->sp -= count;
    for (uint16_t i = 0; i < count; i++) {
        msSetAdd(s, t->sp[i]);
    }
    PUSH(setVal);
    DISPATCH();
}
```

### 5. 类型槽

```c
// set | set → union
static MsValue setOr(MsValue a, MsValue b) {
    if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
    return msSetUnion(a, b);
}

// set & set → intersection
static MsValue setAnd(MsValue a, MsValue b) {
    if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
    return msSetIntersect(a, b);
}

// set - set → difference
static MsValue setSub(MsValue a, MsValue b) {
    if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
    return msSetDiff(a, b);
}

// set ^ set → symmetric difference
static MsValue setXor(MsValue a, MsValue b) {
    if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
    return msSetSymDiff(a, b);
}

// set <= set → subset
static MsValue setLe(MsValue a, MsValue b) {
    if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msSetType) return MS_ERROR_VALUE;
    return MS_BOOL_VAL(msSetIsSubset((MsSetObj*)MS_AS_OBJ(a), (MsSetObj*)MS_AS_OBJ(b)));
}

MsType msSetType = {
    .name = "set", .instanceSize = sizeof(MsSetObj),
    .tp_len      = setLen,
    .tp_eq       = setEq,
    .tp_contains = setContains,
    .tp_iter     = setIter,
    .tp_bitor    = setOr,
    .tp_bitand   = setAnd,
    .tp_sub      = setSub,
    .tp_bitxor   = setXor,
    .tp_le       = setLe,
    .tp_lt       = setLt,   // 真子集
    .tp_mark     = setMark,
    .tp_free     = setFree,
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
    if i in s { found += 1 }
}
print(found)  // 1000000
// 目标：> 10M membership checks/sec
```

---

## 风险与边界

- **集合运算返回新 set**（不修改原集合）；原地版本为 `update`/`intersection_update` 等（方法形式）。
- **迭代顺序**：set 是无序的，迭代顺序由内部哈希桶顺序决定（不保证任何特定顺序）。
- **`set` vs `frozenset`（T063）**：`set` 可变（不可 hash），`frozenset` 不可变（可 hash，可作 set/map 键）。`{1,2}` 字面量创建 `set`，`frozenset([1,2])` 创建 `frozenset`。
