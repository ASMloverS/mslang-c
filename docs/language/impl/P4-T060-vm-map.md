# P4-T060 map 类型（开放寻址哈希表 / 键约束）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `map` 运行时类型（`MsMapObj`）：可变键值字典，使用**开放寻址线性探测哈希表**。键必须是可哈希类型（int/float/bool/nil/str/bytes/tuple/frozenset）。支持插入/查找/删除/迭代/比较以及常用方法。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | str 哈希（`msStrHash`） |
| P4-T053 | int 哈希 |
| P4-T050 | `msGCAlloc` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §2.8 map 类型（哈希表实现） |
| `vm.md` | §2 `BUILD_MAP` 指令（1 字节 A 操作数） |
| `gc.md` | §8 精确根枚举（`traverse`/`MsVisitFn`、GC 根保护） |
| `errors.md` | §1 异常层次结构（`TypeError`/`KeyError`） |

---

## 待实现（C 文件）

```
src/runtime/ms_map.c      # MsMapObj + 哈希表实现
include/mslang/ms_map.h   # msNewMap / msMapGet / msMapSet / msMapDel（均带 vm 参数，供 tpHash 分派）
```

---

## 实现要点

### 1. MsMapObj 结构

`nil` 是合法的可哈希键（type-system.md §2.8），因此空槽/tombstone **不可**借用 `key`
的 tag 做哨兵（原方案用 `MS_TAG_NIL` 表空槽会导致 `{nil: v}` 无法查回）。改用显式
`occupied` 标志（对齐 `type-system.md §2.8` 的 `struct MsMap`/`struct MsMapEntry`）；
`hash` 字段在 `occupied == false` 时复用为 tombstone 标记（见 §3），不再需要额外字段。

```c
struct MsMapEntry {
  MsValue  key;
  MsValue  value;
  uint32_t hash;      // occupied 时为缓存 hash；!occupied 时复用为 tombstone 标记
  bool     occupied;  // 是否持有存活键值对
};

struct MsMapObj {
  struct MsObject     head;
  uint32_t            len;         // 实际键值对数
  uint32_t            cap;         // 槽总数（始终是 2 的幂，msNewMap 内部取整）
  uint32_t            tombstones;  // 已删除槽数（影响负载因子计算）
  struct MsMapEntry*  entries;     // 槽数组（GC 非托管，msAlloc/msRealloc 管理）
};
```

### 2. 哈希计算

`tpHash` 签名为 `MsUnaryFn`：`MsValue (*)(struct MsVM* vm, MsValue a)`（`ms_object.h`），
调用必须带 `vm` 参数。`*ok` 为 `false` 时表示键不可哈希，调用方需转换为 `TypeError`。

```c
static uint32_t hashValue(struct MsVM* vm, MsValue v, bool* ok) {
  struct MsType* tp = msTypeOf(v);
  if (!tp || !tp->tpHash) {
    *ok = false;  // unhashable type（如 list、map）
    return 0;
  }
  if (MS_IS_FLOAT(v) && isnan(MS_AS_FLOAT(v))) {
    *ok = false;  // nan 不可作 map 键（type-system.md §2.8）
    return 0;
  }
  MsValue h = tp->tpHash(vm, v);
  *ok = true;
  return MS_IS_INT(h) ? (uint32_t)(uint64_t)MS_AS_INT(h) : 0;
}
```

键必须实现 `tpHash` 且非 `nan`；否则 `msMapGet`/`msMapSet`/`msMapDel` 返回
`MS_ERROR_VALUE`（TypeError，T080 placeholder，同 `ms_list.c` 的错误传播约定）。

### 3. 开放寻址（线性探测）

`msMapResize` 为 `.c` 内 `static` 辅助函数（扩容/rehash，不进头文件）。tombstone 通过
`!occupied && hash == MS_MAP_TOMBSTONE_HASH` 标记（复用 `hash` 字段，`occupied` 时该字段
仍是真实缓存哈希，两者不冲突）；从未使用过的槽 `hash` 为 0（`msAlloc` 归零分配）。

```c
#define MS_MAP_TOMBSTONE_HASH UINT32_MAX  // 仅在 !occupied 时用作 tombstone 标记

// 查找：返回对应槽指针（可能是命中的活槽、可复用的 tombstone，或从未用过的空槽）
static struct MsMapEntry* findSlot(struct MsMapEntry* entries, uint32_t cap,
                                    MsValue key, uint32_t hash) {
  uint32_t idx = hash & (cap - 1);
  struct MsMapEntry* tombstone = NULL;
  for (;;) {
    struct MsMapEntry* e = &entries[idx];
    if (!e->occupied) {
      if (e->hash == MS_MAP_TOMBSTONE_HASH) {
        if (!tombstone) {
          tombstone = e;  // 记录首个 tombstone，继续探测
        }
      } else {
        return tombstone ? tombstone : e;  // 从未用过的槽：探测终止
      }
    } else if (e->hash == hash && msValueEqual(e->key, key)) {
      return e;  // 找到
    }
    idx = (idx + 1) & (cap - 1);
  }
}

// 键不可哈希 -> MS_ERROR_VALUE（TypeError）；键不存在 -> MS_NIL_VAL（供 .get() 的
// default 语义复用；m[key] 的 KeyError 由 mapGetItem 单独处理，见 §6）。
MsValue msMapGet(struct MsVM* vm, MsValue mapVal, MsValue key) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(mapVal);
  bool ok;
  uint32_t hash = hashValue(vm, key, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  if (!m->len) {
    return MS_NIL_VAL;
  }
  struct MsMapEntry* e = findSlot(m->entries, m->cap, key, hash);
  return e->occupied ? e->value : MS_NIL_VAL;
}

MsValue msMapSet(struct MsVM* vm, MsValue mapVal, MsValue key, MsValue val) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(mapVal);
  bool ok;
  uint32_t hash = hashValue(vm, key, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  // 负载因子 > 0.75（含 tombstone）→ 扩容
  if ((m->len + m->tombstones + 1) * 4 > m->cap * 3) {
    msMapResize(m, m->cap < 8 ? 8 : m->cap * 2);
  }
  struct MsMapEntry* e = findSlot(m->entries, m->cap, key, hash);
  bool isNew = !e->occupied;
  if (isNew && e->hash == MS_MAP_TOMBSTONE_HASH) {
    m->tombstones--;
  }
  e->key      = key;
  e->value    = val;
  e->hash     = hash;
  e->occupied = true;
  if (isNew) {
    m->len++;
  }
  return MS_NIL_VAL;
}

MsValue msMapDel(struct MsVM* vm, MsValue mapVal, MsValue key) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(mapVal);
  bool ok;
  uint32_t hash = hashValue(vm, key, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsMapEntry* e = m->len ? findSlot(m->entries, m->cap, key, hash) : NULL;
  if (!e || !e->occupied) {
    return MS_ERROR_VALUE;  // KeyError (T080 placeholder)
  }
  e->occupied = false;
  e->hash     = MS_MAP_TOMBSTONE_HASH;
  m->len--;
  m->tombstones++;
  return MS_NIL_VAL;
}
```

### 4. GC 支持

项目 GC 模型只有 `MsType.traverse`（`MsTraverseFn`）一个槽位，无 `mark`/`markObject`
（后者是 `ms_gc.c` 内 file-static 函数，`ms_map.c` 无法调用；`ms_object.h` / `gc.md §8`
均以 `traverse` + `MsVisitFn visit` 为准，同 `ms_list.c` 的 `listTraverse`）。

```c
// traverse: 对每个存活槽的 key/value 各调用一次 visit（地址而非值），供未来的
// 半区复制 GC（T116）就地改写。
static void mapTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsMapObj* m = (struct MsMapObj*) obj;
  for (uint32_t i = 0; i < m->cap; i++) {
    struct MsMapEntry* e = &m->entries[i];
    if (e->occupied) {
      visit(&e->key, ctx);
      visit(&e->value, ctx);
    }
  }
}

static void mapDestroy(struct MsObject* obj) {
  msFree(((struct MsMapObj*) obj)->entries);
}
```

### 5. VM 指令

`OP_BUILD_MAP` 为 FMT_A（**1 字节** A 操作数，`vm.md §2`），编译器以
`msChunkEmitOpA(..., OP_BUILD_MAP, (uint8_t) count, ...)` 发射（`ms_compiler.c`），
`ms_disasm.c` 标注 `{"OP_BUILD_MAP", FMT_A}`；须用 `READ_BYTE()` 读取，同
`OP_BUILD_LIST`（`ms_vm.c`）。`msMapSet` 内部 `msMapResize` 可能触发 GC，故构建期间
不提前移动 `t->sp`（key/val 仍在 `[base, sp)` 内被栈扫描覆盖），并显式 `msGCPushRoot`
保护 `map` 本身。

```c
// OP_BUILD_MAP [1B: count]（count 为键值对数，栈上有 2*count 个值，key/val 交替，
// 栈底先入）
case OP_BUILD_MAP: {
  uint8_t count = READ_BYTE();
  MsValue* pairs = t->sp - (uint32_t) count * 2;  // 尚未出栈，仍被 root 扫描覆盖
  MsValue map = msNewMap(count);  // 内部取整到 >= count 的 2 的幂（最小 8）
  msGCPushRoot(map);
  for (uint8_t i = 0; i < count; i++) {
    MsValue r = msMapSet(vm, map, pairs[i * 2], pairs[i * 2 + 1]);
    if (MS_IS_ERROR(r)) {
      msGCPopRoot();
      return r;  // TypeError：键不可哈希
    }
  }
  msGCPopRoot();
  t->sp = pairs;
  PUSH(map);
  DISPATCH();
}
```

### 6. map 方法

`m[key]`（`tpGetitem`）与 `.get(key, default)` 语义不同：前者键不存在须抛
`KeyError`（`errors.md §1`），后者返回 `default`。`msMapGet`（§3）是后者的底层
helper（缺失返回 `MS_NIL_VAL`）；`tpGetitem` 需要单独的 `mapGetItem`：

```c
// m[key] 对应的 tpGetitem：键不存在 -> KeyError（区别于 msMapGet/.get() 的 nil 默认值）
static MsValue mapGetItem(struct MsVM* vm, MsValue mapVal, MsValue key) {
  struct MsMapObj* m = (struct MsMapObj*) MS_AS_OBJ(mapVal);
  bool ok;
  uint32_t hash = hashValue(vm, key, &ok);
  if (!ok) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsMapEntry* e = m->len ? findSlot(m->entries, m->cap, key, hash) : NULL;
  if (!e || !e->occupied) {
    return MS_ERROR_VALUE;  // KeyError (T080 placeholder)
  }
  return e->value;
}
```

| 方法 | 签名 | 说明 |
|---|---|---|
| `get(key, default=nil)` | 安全查找 | 键不存在返回 default |
| `keys()` | `() → list` | 键列表（遍历顺序不保证，见「风险与边界」） |
| `values()` | `() → list` | 值列表 |
| `items()` | `() → list[(k,v)]` | 键值对列表 |
| `has(key)` | `(key) → bool` | 等同 `key in map` |
| `pop(key, default=nil)` | 移除并返回 | |
| `update(other)` | 合并另一个 map | |
| `clear()` | 清空 | |
| `copy()` | 浅拷贝 | |
| `setDefault(key, val)` | 若不存在则设置 | |

---

## 验收标准（checklist）

- [ ] `{"a": 1}["a"]` → 1。
- [ ] `m = {}; m["x"] = 42; m["x"]` → 42。
- [ ] `"a" in {"a": 1}` → true；`"b" in {"a": 1}` → false。
- [ ] `del m["a"]` → 键被移除。
- [ ] `m.get("missing", 0)` → 0。
- [ ] 整数键：`{1: "one", 2: "two"}[2]` → "two"。
- [ ] 列表作 map key → TypeError（unhashable）。
- [ ] `{math.nan: 1}` → TypeError（nan 不可作 map 键）。
- [ ] `m = {"a": 1}; m["missing"]` → KeyError（区别于 `m.get("missing")` 返回 nil）。
- [ ] `{nil: 1}[nil]` → 1（`nil` 是合法键，不与空槽冲突）。
- [ ] 负载因子超 0.75 时自动扩容（`len` 正确，无数据丢失）。
- [ ] GC：map 中的键、值对象均被 `mapTraverse` 正确访问（不被错误回收）。

---

## 测试用例（C 单测）

### `tests/vm/test_map.c`

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

static void testMapBasic(void) {
  MsValue v = run("m := {\"a\": 1, \"b\": 2}\nm[\"a\"]");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 1, "map[\"a\"]=1");

  v = run("len({\"x\": 1, \"y\": 2})");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 2, "len=2");
}

int main(void) {
  MS_RUN(testMapBasic);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
// 创建与访问
scores := {"alice": 95, "bob": 87, "carol": 92}
print(scores["alice"])   // 95
print(scores.get("dave", 0))  // 0（默认值）

// 修改
scores["dave"] = 78
del scores["bob"]
print(len(scores))   // 3

// 迭代
for k, v in scores.items() {
    print($"{k}: {v}")
}

// 嵌套
matrix := {
    "row0": [1, 2, 3],
    "row1": [4, 5, 6],
}
print(matrix["row1"][2])   // 6

// 推导式（T098 map/filter 后）
doubled := {k: v * 2 for k, v in scores.items()}
```

---

## Benchmark

```ms
// benchmarks/bench_map.ms
n := 1_000_000
m := {}
for i in range(n) {
    m[i] = i * 2
}
// 插入性能目标：> 3M inserts/sec

for i in range(n) {
    _ = m[i]
}
// 查找性能目标：> 10M lookups/sec
```

---

## 风险与边界

- **遍历顺序**：`type-system.md §2.8` 未规定 map 保序；开放寻址哈希表本身不保证插入
  顺序，`keys()`/`values()`/`items()` 按 `entries` 槽位物理顺序遍历，顺序不保证跨
  `set`/`del`/resize 稳定（如需 Python 3.7+ 式保序，需先在设计文档中新增该语义并追加
  `insertOrder` 数组，超出本任务范围）。
- **rehash 时 tombstone**：resize 时重建 `entries` 数组，tombstone 自然消除（重新哈希所有存活键）。
- **GC 与 map 修改**：`OP_BUILD_MAP` 与 `msMapSet` 在 `msMapResize`（含 `msAlloc`）期间
  若触发 GC，需保护 map 对象（`msGCPushRoot`，见 §5 VM 指令）。
