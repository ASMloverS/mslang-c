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
| P4-T060 | map 实现（验收标准需要 tuple 作为 map 键） |
| P4-T057 | str 哈希（`msFnv1a32`，tuple 哈希聚合复用） |
| P4-T050 | `msGCAlloc` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §2.9 tuple 类型、§1.3 `MsType`/`MsVisitFn` |
| `vm.md` | `BUILD_TUPLE` 指令（1 字节 A 操作数） |
| `gc.md` | traverse/`varSize` 模型（`ms_gc.c` 印证） |

---

## 待实现（C 文件）

```
src/runtime/ms_tuple.c     # MsTupleObj + 类型槽
include/mslang/ms_tuple.h  # msNewTuple / msNewTupleN
```

---

## 实现要点

### 1. MsTupleObj 结构

对齐 `type-system.md §2.9` 的 `struct MsTuple`（`head`/`len`/`items[]`），并追加 `hash`
缓存字段（同 `struct MsStr` 的 `hash`，`type-system.md §2.5`，0 表示未计算）。不 typedef
（`c-style.md §4.2`）。`items[]` 由调用方在分配后、下一次可能触发 GC 的分配之前完整
填充（见 `msNewTuple` 注释）；`msGCAlloc` 之后到写完 `items` 之前，`tupleTraverse` 一旦
被 GC 触发会读到未初始化的槽位，因此 `msNewTuple` 内部清零 `items`，未完整赋值前的槽
位遍历安全（值为 `MS_NIL_VAL` 的位模式，非悬空指针）。

```c
struct MsTupleObj {
  struct MsObject head;
  uint32_t        len;
  uint32_t        hash;   // FNV-1a 缓存，0 = 未计算（同 struct MsStr）
  MsValue         items[];  // 内联存储（flexible array member）
};

static size_t tupleVarSize(const struct MsObject* obj) {
  return sizeof(struct MsTupleObj) + ((const struct MsTupleObj*) obj)->len * sizeof(MsValue);
}

// 分配大小 = sizeof(MsTupleObj) + len * sizeof(MsValue)；items 清零，
// 保证分配后、调用方填充前若发生 GC，tupleTraverse 不读到垃圾指针。
MsValue msNewTuple(uint32_t len) {
  size_t size = sizeof(struct MsTupleObj) + (size_t) len * sizeof(MsValue);
  struct MsTupleObj* t = (struct MsTupleObj*) msGCAlloc(&msTupleType, size);
  t->len = len;
  t->hash = 0;
  memset(t->items, 0, (size_t) len * sizeof(MsValue));
  return MS_OBJ_VAL(t);
}
```

### 2. 哈希计算

`tpHash` 签名为 `MsUnaryFn`：`MsValue (*)(struct MsVM* vm, MsValue a)`（`ms_object.h`），
调用元素的 `tpHash` 时必须带 `vm` 参数（同 T060 review 的同类问题）。

```c
static MsValue tupleHash(struct MsVM* vm, MsValue v) {
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  if (t->hash) {
    return MS_INT_VAL((int64_t) (uint32_t) t->hash);
  }

  // FNV-1a 聚合所有元素的 hash
  uint32_t h = 2166136261u;
  for (uint32_t i = 0; i < t->len; i++) {
    struct MsType* tp = msTypeOf(t->items[i]);
    if (!tp->tpHash) {
      return MS_ERROR_VALUE;  // 元素不可哈希 → TypeError
    }
    MsValue eh = tp->tpHash(vm, t->items[i]);
    if (MS_IS_ERROR(eh)) {
      return MS_ERROR_VALUE;
    }
    uint32_t ev = (uint32_t) (uint64_t) MS_AS_INT(eh);
    h = (h ^ ev) * 16777619u;
  }
  if (!h) {
    h = 1;
  }
  t->hash = h;
  return MS_INT_VAL((int64_t) (uint32_t) h);
}
```

### 3. 类型槽

`MsType` 无 `tpMark`/`tpFree` 槽（`type-system.md §1.3`）；GC 遍历统一走
`traverse`（`MsTraverseFn`：`void (*)(struct MsObject*, MsVisitFn visit, void* ctx)`），
对齐 `ms_list.c` 的 `listTraverse`。变长对象须提供 `varSize`（§1，`tupleVarSize`），
否则 `ms_gc.c` 按 `objSize` 计账会得到错误大小（`objSize` 仅用于 GC 找不到 `varSize`
时的定长对象兜底，此处仍设为 `sizeof(struct MsTupleObj)` 表示表头大小）。`tpIter`
延后到 T065（同 `ms_list.c` 的既定策略），本任务不挂该槽；`x, y := point` 解包走
赋值编译期的固定元素展开，不依赖迭代协议。

```c
static MsValue tupleLen(struct MsVM* vm, MsValue v) {
  (void) vm;
  return MS_INT_VAL((int64_t) ((struct MsTupleObj*) MS_AS_OBJ(v))->len);
}

static bool isTuple(MsValue v) {
  return MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msTupleType;
}

static MsValue tupleEq(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isTuple(b)) {
    return MS_BOOL_VAL(false);
  }
  struct MsTupleObj* ta = (struct MsTupleObj*) MS_AS_OBJ(a);
  struct MsTupleObj* tb = (struct MsTupleObj*) MS_AS_OBJ(b);
  if (ta->len != tb->len) {
    return MS_BOOL_VAL(false);
  }
  for (uint32_t i = 0; i < ta->len; i++) {
    if (!msValueEqual(ta->items[i], tb->items[i])) {
      return MS_BOOL_VAL(false);
    }
  }
  return MS_BOOL_VAL(true);
}

// 字典序比较（同 strLt 的思路）：逐元素比较到首个不等处；不可比元素 -> TypeError；
// 公共前缀相等时较短者 < 较长者。
static MsValue tupleLt(struct MsVM* vm, MsValue a, MsValue b) {
  if (!isTuple(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsTupleObj* ta = (struct MsTupleObj*) MS_AS_OBJ(a);
  struct MsTupleObj* tb = (struct MsTupleObj*) MS_AS_OBJ(b);
  uint32_t n = ta->len < tb->len ? ta->len : tb->len;
  for (uint32_t i = 0; i < n; i++) {
    if (msValueEqual(ta->items[i], tb->items[i])) {
      continue;
    }
    struct MsType* tp = msTypeOf(ta->items[i]);
    if (!tp->tpLt) {
      return MS_ERROR_VALUE;  // TypeError: 元素不可比较
    }
    return tp->tpLt(vm, ta->items[i], tb->items[i]);
  }
  return MS_BOOL_VAL(ta->len < tb->len);
}

static MsValue tupleGetItem(struct MsVM* vm, MsValue v, MsValue idx) {
  (void) vm;
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  if (!MS_IS_INT(idx)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  int64_t i = MS_AS_INT(idx);
  if (i < 0) {
    i += (int64_t) t->len;
  }
  if (i < 0 || i >= (int64_t) t->len) {
    return MS_ERROR_VALUE;  // IndexError (T080 placeholder)
  }
  return t->items[i];
}

static MsValue tupleContains(struct MsVM* vm, MsValue v, MsValue item) {
  (void) vm;
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
  for (uint32_t i = 0; i < t->len; i++) {
    if (msValueEqual(t->items[i], item)) {
      return MS_BOOL_VAL(true);
    }
  }
  return MS_BOOL_VAL(false);
}

// (1,2) + (3,) -> (1,2,3)；同 listConcat 的类型校验/溢出校验思路。
static MsValue tupleConcat(struct MsVM* vm, MsValue a, MsValue b) {
  (void) vm;
  if (!isTuple(b)) {
    return MS_ERROR_VALUE;  // TypeError (T080 placeholder)
  }
  struct MsTupleObj* ta = (struct MsTupleObj*) MS_AS_OBJ(a);
  struct MsTupleObj* tb = (struct MsTupleObj*) MS_AS_OBJ(b);
  if ((uint64_t) ta->len + tb->len > UINT32_MAX) {
    return MS_ERROR_VALUE;  // OverflowError (T080 placeholder)
  }
  uint32_t newLen = ta->len + tb->len;
  MsValue r = msNewTuple(newLen);
  struct MsTupleObj* tr = (struct MsTupleObj*) MS_AS_OBJ(r);
  if (ta->len) {
    memcpy(tr->items, ta->items, ta->len * sizeof(MsValue));
  }
  if (tb->len) {
    memcpy(tr->items + ta->len, tb->items, tb->len * sizeof(MsValue));
  }
  return r;
}

// traverse: 对每个槽的 item 调用一次 visit（地址而非值），同 listTraverse。
static void tupleTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsTupleObj* t = (struct MsTupleObj*) obj;
  for (uint32_t i = 0; i < t->len; i++) {
    visit(&t->items[i], ctx);
  }
}

struct MsType msTupleType = {
    .name = "tuple",
    .objSize = sizeof(struct MsTupleObj),
    .varSize = tupleVarSize,
    .traverse = tupleTraverse,
    .destroy = NULL,  // flexible array 内联存储，msGCAlloc 分配的整块随 obj 一并回收
    .tpLen = tupleLen,
    .tpEq = tupleEq,
    .tpLt = tupleLt,
    .tpHash = tupleHash,
    .tpGetitem = tupleGetItem,
    .tpContains = tupleContains,
    .tpAdd = tupleConcat,  // (1,2) + (3,) → (1,2,3)
};
```

### 4. VM 指令

`OP_BUILD_TUPLE` 为 FMT_A（**1 字节** A 操作数），编译器 `compileContainerElems`
以 `msChunkEmitOpA(..., (uint8_t) count, ...)` 发射（`ms_compiler.c`），
`ms_disasm.c` 标注 `{"OP_BUILD_TUPLE", FMT_A}`；须 `READ_BYTE()` 读取，同
`OP_BUILD_LIST`（`ms_vm.c`）。`t` 是 VM 线程/栈所有者（`struct MsThread*`，同
`OP_BUILD_LIST` 用法），不可与新建的 tuple 对象指针同名——tuple 对象本身没有
`sp` 字段。

```c
// OP_BUILD_TUPLE [1B: count]
case OP_BUILD_TUPLE: {
  uint8_t count = READ_BYTE();
  MsValue val = msNewTuple(count);
  struct MsTupleObj* tup = (struct MsTupleObj*) MS_AS_OBJ(val);
  t->sp -= count;  // t = VM 线程/栈所有者
  if (count) {
    memcpy(tup->items, t->sp, count * sizeof(MsValue));
  }
  PUSH(val);
  DISPATCH();
}
```

### 5. tuple 方法（只读子集）

| 方法 | 说明 |
|---|---|
| `index(v)` | 首次出现的索引 |
| `count(v)` | 出现次数 |
| `len()` | 等同 `tpLen` |

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
  struct MsTupleObj* t = (struct MsTupleObj*) MS_AS_OBJ(v);
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
func minmax(lst) {
    return min(lst), max(lst)
}
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
- **`msNewTuple` 使用 flexible array**：`msGCAlloc` 分配 `sizeof(struct MsTupleObj) + n * sizeof(MsValue)` 字节；`ms_gc.c` 的 sweep 阶段无条件对每个对象调用 `msFree(obj)`，`.destroy` 仅用于释放对象自身之外的额外分配（如 list 的 `items` 缓冲），tuple 数据全部内联，故 `.destroy = NULL`，`varSize` 保证 sweep 用正确大小记账。
- **空 tuple**：`()` 是合法 tuple（len=0），`msNewTuple(0)` 须正确处理；可以对空 tuple 使用单例（全局 `gEmptyTuple`，避免重复分配）。
