# P4-T063 frozenset 类型（不可变集合 / 可哈希）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `frozenset` 运行时类型（`MsFrozensetObj`）：不可变集合，元素必须可哈希。与 `set` 的区别：`frozenset` 本身实现了 `tpHash`，因此可以作为 `map`/`set` 的键。内存布局使用内联数组（同 tuple）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T062 | `set` 实现（frozenset 操作集与 set 高度重叠） |
| P4-T050 | `msGCAlloc` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §14 frozenset 类型 |

---

## 待实现（C 文件）

```
src/runtime/ms_frozenset.c    # MsFrozensetObj + 类型槽
include/mslang/ms_frozenset.h
```

---

## 实现要点

### 1. 结构

```c
// 内联哈希表（与 set 类似，但不可修改）
typedef struct MsFrozensetEntry {
  MsValue  key;
  uint32_t hash;
} MsFrozensetEntry;

typedef struct MsFrozensetObj {
  MsObject  header;
  uint32_t  count;
  uint32_t  cap;
  uint32_t  hashVal;     // frozenset 整体哈希（0=未计算）
  MsFrozensetEntry entries[];  // 内联存储
} MsFrozensetObj;

// 构造：从 set 或 iterable 创建
MsValue msNewFrozenset(MsSetObj* src);
```

### 2. 哈希计算（XOR 聚合，顺序无关）

```c
static MsValue frozensetHash(MsValue v) {
  MsFrozensetObj* fs = (MsFrozensetObj*)MS_AS_OBJ(v);
  if (fs->hashVal) return MS_INT_VAL((int64_t)(uint32_t)fs->hashVal);

  uint32_t h = 0;
  for (uint32_t i = 0; i < fs->cap; i++) {
    if (!MS_IS_NIL(fs->entries[i].key) && !MS_IS_ERROR(fs->entries[i].key)) {
      // XOR 聚合（顺序无关，适合集合语义）
      h ^= fs->entries[i].hash * 0x9e3779b9u;
    }
  }
  if (!h) h = 1;
  fs->hashVal = h;
  return MS_INT_VAL((int64_t)(uint32_t)h);
}
```

### 3. 类型槽（只读操作与 set 相同，去掉修改方法）

```c
MsType msFrozensetType = {
  .name = "frozenset", .instanceSize = 0,
  .tpLen      = frozensetLen,
  .tpEq       = frozensetEq,
  .tpLt       = frozensetLt,    // 真子集
  .tpLe       = frozensetLe,    // 子集
  .tpHash     = frozensetHash,  // 可哈希！
  .tpContains = frozensetContains,
  .tpIter     = frozensetIter,
  .tpBitor    = frozensetUnion,
  .tpBitand   = frozensetIntersect,
  .tpSub      = frozensetDiff,
  .tpBitxor   = frozensetSymDiff,
  .tpMark     = frozensetMark,
  .tpFree     = NULL,  // 内联存储，随 header 释放
};
```

### 4. frozenset 方法（只读）

- `union(other)` / `intersection(other)` / `difference(other)` / `symmetricDifference(other)`
- `isSubset(other)` / `isSuperset(other)` / `isDisjoint(other)`
- `copy()` → 返回自身（不可变，无需真正复制）

---

## 验收标准（checklist）

- [ ] `frozenset([1, 2, 3])` → 长度为 3 的 frozenset。
- [ ] `hash(frozenset([1,2,3]))` → int（无 TypeError）。
- [ ] `frozenset([1,2,3]) == frozenset([3,2,1])` → true（集合相等，无序）。
- [ ] `{frozenset([1,2]): "key"}` → frozenset 可作 map 键。
- [ ] `1 in frozenset([1,2,3])` → true。
- [ ] `frozenset([1,2]) | frozenset([2,3])` → 返回新 frozenset，len=3。
- [ ] frozenset 无 `add`/`remove` 方法（调用 → AttributeError）。

---

## 测试用例（.ms）

```ms
fs := frozenset([1, 2, 3])
print(1 in fs)            // true
print(hash(fs))           // 某整数（可哈希）

// 作为 map 键
cache := {}
cache[frozenset([1,2])] = "pair"
print(cache[frozenset([2,1])])  // pair（集合相等）

// 集合运算
a := frozenset([1, 2, 3])
b := frozenset([2, 3, 4])
print(a | b)   // frozenset({1, 2, 3, 4})
print(a & b)   // frozenset({2, 3})
```

---

## Benchmark

N/A（frozenset 主要用于 dict key，性能归入 map bench）。

---

## 风险与边界

- **`copy()` 返回 self**：frozenset 不可变，`copy()` 返回自身引用（不分配新对象），符合 Python 语义。
- **与 set 的混合运算**：`frozenset | set` 应返回 frozenset（Python 规则：左操作数决定类型）；初版可不支持，仅同类型运算。
