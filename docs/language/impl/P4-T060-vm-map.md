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
| `type-system.md` | §11 map 类型（哈希表实现） |

---

## 待实现（C 文件）

```
src/runtime/ms_map.c      # MsMapObj + 哈希表实现
include/mslang/ms_map.h   # msNewMap / msMapGet / msMapSet / msMapDel
```

---

## 实现要点

### 1. MsMapObj 结构

```c
typedef struct MsMapEntry {
  MsValue key;    // MS_TAG_NIL 表示空槽，MS_TAG_ERROR 表示已删除（tombstone）
  MsValue val;
  uint32_t hash;  // 缓存的 hash 值（避免重新计算）
} MsMapEntry;

typedef struct MsMapObj {
  MsObject   header;
  uint32_t   count;    // 实际键值对数
  uint32_t   cap;      // 槽总数（始终是 2 的幂）
  uint32_t   tombstones; // 已删除槽数（影响负载因子计算）
  MsMapEntry* entries; // 槽数组（GC 非托管）
} MsMapObj;
```

### 2. 哈希计算

```c
static uint32_t hashValue(MsValue v) {
  MsType* tp = msTypeOf(v);
  if (!tp || !tp->tpHash) return 0;
  MsValue h = tp->tpHash(v);
  return MS_IS_INT(h) ? (uint32_t)(uint64_t)MS_AS_INT(h) : 0;
}
```

键必须实现 `tpHash`；未实现（如 list、map）→ TypeError（"unhashable type"）。

### 3. 开放寻址（线性探测）

```c
// 查找：返回对应槽指针（可能为空槽或已删除槽）
static MsMapEntry* findSlot(MsMapEntry* entries, uint32_t cap,
                             MsValue key, uint32_t hash) {
  uint32_t idx = hash & (cap - 1);
  MsMapEntry* tombstone = NULL;
  for (;;) {
    MsMapEntry* e = &entries[idx];
    if (MS_IS_NIL(e->key)) {
      // 空槽：若有 tombstone 先用 tombstone
      return tombstone ? tombstone : e;
    }
    if (MS_IS_ERROR(e->key)) {
      // tombstone：记录但继续探测
      if (!tombstone) tombstone = e;
    } else if (e->hash == hash && msValueEqual(e->key, key)) {
      return e;  // 找到
    }
    idx = (idx + 1) & (cap - 1);
  }
}

MsValue msMapGet(MsValue mapVal, MsValue key) {
  MsMapObj* m = (MsMapObj*)MS_AS_OBJ(mapVal);
  if (!m->count) return MS_NIL_VAL;
  uint32_t hash = hashValue(key);
  MsMapEntry* e = findSlot(m->entries, m->cap, key, hash);
  if (MS_IS_NIL(e->key) || MS_IS_ERROR(e->key)) return MS_NIL_VAL;
  return e->val;
}

void msMapSet(MsValue mapVal, MsValue key, MsValue val) {
  MsMapObj* m = (MsMapObj*)MS_AS_OBJ(mapVal);
  // 负载因子 > 0.75（含 tombstone）→ 扩容
  if ((m->count + m->tombstones + 1) * 4 > m->cap * 3) {
    msMapResize(m, m->cap < 8 ? 8 : m->cap * 2);
  }
  uint32_t hash = hashValue(key);
  MsMapEntry* e = findSlot(m->entries, m->cap, key, hash);
  bool isNew = MS_IS_NIL(e->key) || MS_IS_ERROR(e->key);
  if (isNew && MS_IS_ERROR(e->key)) m->tombstones--;
  e->key  = key;
  e->val  = val;
  e->hash = hash;
  if (isNew) m->count++;
}

void msMapDel(MsValue mapVal, MsValue key) {
  MsMapObj* m = (MsMapObj*)MS_AS_OBJ(mapVal);
  uint32_t hash = hashValue(key);
  MsMapEntry* e = findSlot(m->entries, m->cap, key, hash);
  if (MS_IS_NIL(e->key) || MS_IS_ERROR(e->key)) return;  // KeyError（T080 后完整报错）
  e->key = MS_ERROR_VALUE;   // tombstone
  m->count--;
  m->tombstones++;
}
```

### 4. GC 支持

```c
static void mapMark(MsObject* obj) {
  MsMapObj* m = (MsMapObj*)obj;
  for (uint32_t i = 0; i < m->cap; i++) {
    MsMapEntry* e = &m->entries[i];
    if (!MS_IS_NIL(e->key) && !MS_IS_ERROR(e->key)) {
      if (MS_IS_OBJ(e->key)) markObject(MS_AS_OBJ(e->key));
      if (MS_IS_OBJ(e->val)) markObject(MS_AS_OBJ(e->val));
    }
  }
}
static void mapFree(MsObject* obj) { msFree(((MsMapObj*)obj)->entries); }
```

### 5. VM 指令

```c
// OP_BUILD_MAP [2B: count]（count 为键值对数，栈上有 2*count 个值）
case OP_BUILD_MAP: {
  uint16_t count = READ_U16();
  MsValue map = msNewMap(count * 2);  // 初始容量为 2*count
  t->sp -= count * 2;
  for (uint16_t i = 0; i < count; i++) {
    msMapSet(map, t->sp[i*2], t->sp[i*2+1]);
  }
  PUSH(map);
  DISPATCH();
}
```

### 6. map 方法

| 方法 | 签名 | 说明 |
|---|---|---|
| `get(key, default=nil)` | 安全查找 | 键不存在返回 default |
| `keys()` | `() → list` | 键列表（插入顺序，Python 3.7+） |
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
- [ ] 负载因子超 0.75 时自动扩容（`count` 正确，无数据丢失）。
- [ ] 插入顺序保留（`keys()` 返回插入顺序）。
- [ ] GC：map 中的值对象被正确 mark（不被错误回收）。

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
scores["dave"] := 78
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
doubled := {k: v*2 for k, v in scores.items()}
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

- **插入顺序**：开放寻址不天然保证插入顺序；v1 使用单独的 `insertOrder` 数组（`uint32_t* insertOrder`，存储 entries 下标顺序）来保证 `keys()`/`items()` 返回插入顺序。开销约 4B/entry，可接受。
- **rehash 时 tombstone**：resize 时重建 `entries` 数组，tombstone 自然消除（重新哈希所有存活键）。
- **GC 与 map 修改**：`msMapSet` 在 `msMapResize`（含 `msAlloc`）期间若触发 GC，需保护 map 对象（`msGCPushRoot`）。
