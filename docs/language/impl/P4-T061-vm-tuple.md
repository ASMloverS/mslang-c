# P4-T061 tuple 类型（不可变序列 / hash 支持）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `tuple` 运行时类型（`MsTupleObj`）：不可变有序序列，元素可以是任意类型。与 list 的关键区别：tuple 不可变，因此可以作为 map/set 的键（若所有元素可哈希）。内存布局使用内联数组（flexible array member）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T059 | list 实现（tuple 方法集是 list 的子集） |
| P4-T050 | `msGCAlloc` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §12 tuple 类型 |

---

## 待实现（C 文件）

```
src/runtime/ms_tuple.c     # MsTupleObj + 类型槽
include/mslang/ms_tuple.h  # msNewTuple / msNewTupleN
```

---

## 实现要点

### 1. MsTupleObj 结构

```c
typedef struct MsTupleObj {
    MsObject  header;
    uint32_t  len;
    uint32_t  hashVal;  // 0 = 未计算
    MsValue   items[];  // 内联存储（flexible array member）
} MsTupleObj;

// 分配大小 = sizeof(MsTupleObj) + len * sizeof(MsValue)
MsValue msNewTuple(uint32_t len) {
    size_t size = sizeof(MsTupleObj) + len * sizeof(MsValue);
    MsTupleObj* t = (MsTupleObj*)msGCAlloc(&msTupleType, size);
    t->len     = len;
    t->hashVal = 0;
    return MS_OBJ_VAL(t);
}
```

### 2. 哈希计算

```c
static MsValue tupleHash(MsValue v) {
    MsTupleObj* t = (MsTupleObj*)MS_AS_OBJ(v);
    if (t->hashVal) return MS_INT_VAL((int64_t)(uint32_t)t->hashVal);

    // FNV-1a 聚合所有元素的 hash
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < t->len; i++) {
        MsType* tp = msTypeOf(t->items[i]);
        if (!tp->tp_hash) return MS_ERROR_VALUE;  // 元素不可哈希 → TypeError
        MsValue eh = tp->tp_hash(t->items[i]);
        if (MS_IS_ERROR(eh)) return MS_ERROR_VALUE;
        uint32_t ev = (uint32_t)(uint64_t)MS_AS_INT(eh);
        h = (h ^ ev) * 16777619u;
    }
    if (!h) h = 1;
    t->hashVal = h;
    return MS_INT_VAL((int64_t)(uint32_t)h);
}
```

### 3. 类型槽

```c
static MsValue tupleLen(MsValue v) {
    return MS_INT_VAL(((MsTupleObj*)MS_AS_OBJ(v))->len);
}

static MsValue tupleEq(MsValue a, MsValue b) {
    if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msTupleType) return MS_BOOL_VAL(false);
    MsTupleObj* ta = (MsTupleObj*)MS_AS_OBJ(a);
    MsTupleObj* tb = (MsTupleObj*)MS_AS_OBJ(b);
    if (ta->len != tb->len) return MS_BOOL_VAL(false);
    for (uint32_t i = 0; i < ta->len; i++) {
        if (!msValueEqual(ta->items[i], tb->items[i])) return MS_BOOL_VAL(false);
    }
    return MS_BOOL_VAL(true);
}

static MsValue tupleGetItem(MsValue v, MsValue idx) {
    MsTupleObj* t = (MsTupleObj*)MS_AS_OBJ(v);
    if (!MS_IS_INT(idx)) return MS_ERROR_VALUE;
    int64_t i = MS_AS_INT(idx);
    if (i < 0) i += (int64_t)t->len;
    if (i < 0 || i >= (int64_t)t->len) return MS_ERROR_VALUE;  // IndexError
    return t->items[i];
}

static void tupleMark(MsObject* obj) {
    MsTupleObj* t = (MsTupleObj*)obj;
    for (uint32_t i = 0; i < t->len; i++) {
        if (MS_IS_OBJ(t->items[i])) markObject(MS_AS_OBJ(t->items[i]));
    }
}

MsType msTupleType = {
    .name = "tuple", .instanceSize = 0,  // 动态大小
    .tp_len      = tupleLen,
    .tp_eq       = tupleEq,
    .tp_lt       = tupleLt,   // 字典序比较
    .tp_hash     = tupleHash,
    .tp_getitem  = tupleGetItem,
    .tp_iter     = tupleIter,
    .tp_contains = tupleContains,
    .tp_add      = tupleConcat,  // (1,2) + (3,) → (1,2,3)
    .tp_mark     = tupleMark,
    .tp_free     = NULL,  // flexible array 随 header 释放
};
```

### 4. VM 指令

```c
// OP_BUILD_TUPLE [2B: count]
case OP_BUILD_TUPLE: {
    uint16_t count = READ_U16();
    MsValue tup = msNewTuple(count);
    MsTupleObj* t = (MsTupleObj*)MS_AS_OBJ(tup);
    t->sp -= count;
    for (uint16_t i = 0; i < count; i++) t->items[i] = t->sp[i];
    PUSH(tup);
    DISPATCH();
}
```

### 5. tuple 方法（只读子集）

| 方法 | 说明 |
|---|---|
| `index(v)` | 首次出现的索引 |
| `count(v)` | 出现次数 |
| `len()` | 等同 `tp_len` |

---

## 验收标准（checklist）

- [ ] `(1, 2, 3)[1]` → 2。
- [ ] `(1, 2) + (3,)` → `(1, 2, 3)`。
- [ ] `len((1, 2, 3))` → 3。
- [ ] `(1, "a") == (1, "a")` → true。
- [ ] `hash((1, 2))` 可以调用（返回 int）。
- [ ] `{(1, 2): "ok"}[(1, 2)]` → "ok"（tuple 作为 map 键）。
- [ ] `(1, [2])` 的 `hash` → TypeError（内含 list，不可哈希）。
- [ ] tuple 不可修改：`t[0] = 1` → TypeError。
- [ ] `2 in (1, 2, 3)` → true。
- [ ] `(1, 2) < (1, 3)` → true（字典序）。

---

## 测试用例（C 单测）

### `tests/vm/test_tuple.c`

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

static void testTupleBuild(void) {
    MsValue v = run("(1, 2, 3)");
    MsTupleObj* t = (MsTupleObj*)MS_AS_OBJ(v);
    MS_ASSERT_TRUE(t->len == 3, "len 3");
    MS_ASSERT_TRUE(MS_AS_INT(t->items[1]) == 2, "t[1]=2");
}

static void testTupleHash(void) {
    MsValue v = run("hash((1, 2, 3))");
    MS_ASSERT_TRUE(MS_IS_INT(v), "hash is int");
}

int main(void) {
    MS_RUN(testTupleBuild);
    MS_RUN(testTupleHash);
    return msTestSummary();
}
```

### .ms 使用示例

```ms
// 基本 tuple
point := (3, 4)
x, y := point    // 解包
print(x, y)      // 3 4

// 单元素 tuple（注意尾逗号）
one := (42,)
print(len(one))  // 1

// tuple 作为 map 键
cache := {}
cache[(1, 2)] = "origin"
print(cache[(1, 2)])  // origin

// 函数多返回值（实际返回 tuple）
func minmax(lst) { return min(lst), max(lst) }
lo, hi := minmax([3, 1, 4, 1, 5, 9])
print(lo, hi)  // 1 9

// 不可变性
t := (1, 2, 3)
// t[0] = 99  → TypeError
```

---

## Benchmark

```ms
// benchmarks/bench_tuple.ms
n := 1_000_000
for i in range(n) {
    t := (i, i+1, i+2)
    _ = t[0] + t[2]
}
// 目标：> 10M tuple ops/sec
```

---

## 风险与边界

- **`OP_BUILD_TUPLE` 与 `OP_BUILD_LIST`**：实现几乎相同，区别在于结果类型和是否可修改。可以共用部分代码（模板宏）。
- **`msNewTuple` 使用 flexible array**：`msGCAlloc` 分配 `sizeof(MsTupleObj) + n * sizeof(MsValue)` 字节；GC free 时直接 `msFree(obj)`（无需单独 `tp_free`），因为所有数据内联。
- **空 tuple**：`()` 是合法 tuple（len=0），`msNewTuple(0)` 须正确处理；可以对空 tuple 使用单例（全局 `gEmptyTuple`，避免重复分配）。
