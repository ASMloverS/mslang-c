# P4-T063 frozenset 类型（不可变集合 / 可哈希）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `frozenset` 运行时类型：不可变集合，元素必须可哈希。与 `set` 的区别：`frozenset` 本身实现了 `tpHash`，因此可以作为 `map`/`set` 的键。内部布局复用 `struct MsSetObj`/`struct MsSetEntry`（T062），通过类型描述符（`head.type`）区分可变性，不新增 C 结构体。

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
| `type-system.md` | §2.11 frozenset |
| `type-system.md` | §2.10 set（复用 MsSetObj/MsSetEntry 结构依据） |
| `type-system.md` | §1.3 MsType（类型槽字段名依据） |

---

## 待实现（C 文件）

```
src/runtime/ms_frozenset.c    # 复用 MsSetObj/MsSetEntry + msFrozensetType 类型槽
include/mslang/ms_frozenset.h
```

---

## 实现要点

### 1. 结构

不新增结构体：复用 T062 的 `struct MsSetObj`/`struct MsSetEntry`（`ms_set.h`），仅通过 `head.type` 指向 `msFrozensetType` 而非 `msSetType` 来区分可变性。

```c
// 从任意可迭代对象（list/set 等，泛型 iterable 支持随 T065 落地）构造 frozenset。
// 复用 msSetAdd 的哈希去重逻辑，逐个插入后返回不可变实例。
MsValue msNewFrozensetFromIter(struct MsVM* vm, MsValue iterable);
```

### 2. 哈希计算（XOR 聚合，顺序无关）

`tpHash` 槽为 `MsUnaryFn`（`type-system.md §1.3`），签名为 `MsValue (*)(struct MsVM* vm, MsValue a)`；哈希缓存字段沿用 T062 的 `struct MsSetObj`，无需新增字段（如需缓存，应先在 `type-system.md §2.10` 补充该字段再落地）。

```c
static MsValue frozensetHash(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsSetObj* fs = (struct MsSetObj*) MS_AS_OBJ(v);

  uint32_t h = 0;
  for (uint32_t i = 0; i < fs->cap; i++) {
    if (fs->entries[i].occupied) {
      // XOR 聚合（顺序无关，适合集合语义）
      h ^= fs->entries[i].hash * 0x9e3779b9u;
    }
  }
  if (!h) h = 1;
  return MS_INT_VAL((int64_t)(uint32_t)h);
}
```

### 3. 类型槽（只读操作与 set 相同，去掉修改方法）

结构复用 `struct MsSetObj`，故 `objSize` 与 `msSetType` 一致；`entries` 为独立堆分配，`traverse`/`destroy` 逻辑与 `ms_set.c` 的 `setTraverse`/`setDestroy` 相同（逐槽 `occupied` 遍历子引用 / 释放 `entries`），可按需导出复用或提供等价实现。字段名对齐 `struct MsType`（`type-system.md §1.3`）。

```c
struct MsType msFrozensetType = {
  .name       = "frozenset",
  .objSize    = sizeof(struct MsSetObj),
  .traverse   = frozensetTraverse,  // 逻辑同 setTraverse
  .destroy    = frozensetDestroy,   // 逻辑同 setDestroy，释放 entries
  .tpLen      = frozensetLen,
  .tpEq       = frozensetEq,
  .tpLt       = frozensetLt,    // 真子集
  .tpLe       = frozensetLe,    // 子集
  .tpHash     = frozensetHash,  // 可哈希！
  .tpContains = frozensetContains,
  .tpBitor    = frozensetUnion,
  .tpBitand   = frozensetIntersect,
  .tpSub      = frozensetDiff,
  .tpBitxor   = frozensetSymDiff,
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
cache[frozenset([1, 2])] = "pair"
print(cache[frozenset([2, 1])])  // pair（集合相等）

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
- **与 set 的混合运算**：仅当两操作数均为 frozenset 时返回 frozenset，否则返回 set（`type-system.md §2.11`）；初版可不支持混合运算，仅同类型运算。
