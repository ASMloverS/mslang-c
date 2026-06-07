# P4-T059 list 类型（动态数组 / 方法 / 切片）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `list` 运行时类型（`MsListObj`）：可变动态数组，元素类型任意。支持索引/切片/迭代/比较/拼接，以及完整的列表方法（`append`/`pop`/`insert`/`remove`/`sort`/`reverse`/`index`/`count`/`extend`/`clear`/`copy`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T056 | 比较协议（用于 sort） |
| P4-T050 | `msGCAlloc` |
| P3-T041 | `OP_BUILD_LIST` / `OP_UNPACK` 指令（VM 侧） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §10 list 类型 |
| `stdlib/collections.md` | list 方法清单 |

---

## 待实现（C 文件）

```
src/runtime/ms_list.c      # MsListObj + 类型槽 + 方法
include/mslang/ms_list.h   # msNewList / msListAppend / msListGet / etc.
```

---

## 实现要点

### 1. MsListObj 结构

```c
typedef struct MsListObj {
  MsObject  header;
  uint32_t  len;       // 元素个数
  uint32_t  cap;       // 容量（已分配的 MsValue 数）
  MsValue*  items;     // MsValue 数组（GC 非托管，手动 realloc）
} MsListObj;
```

### 2. 核心操作

```c
MsValue msNewList(uint32_t initCap);    // 创建空 list
void    msListAppend(MsListObj* l, MsValue v); // append（自动扩容）
MsValue msListGet(MsListObj* l, int64_t i);    // get（支持负索引）
void    msListSet(MsListObj* l, int64_t i, MsValue v);
void    msListInsert(MsListObj* l, int64_t i, MsValue v);
MsValue msListPop(MsListObj* l, int64_t i);   // 移除并返回
MsValue msListSlice(MsListObj* l, int64_t lo, int64_t hi, int64_t step);
```

扩容策略：容量翻倍（`cap < 4 → 4 → 8 → 16 → ...`）。

### 3. GC 支持

```c
// tpMark：遍历 items，对每个 OBJ 元素调用 markObject
static void listMark(MsObject* obj) {
  MsListObj* l = (MsListObj*)obj;
  for (uint32_t i = 0; i < l->len; i++) {
    if (MS_IS_OBJ(l->items[i])) markObject(MS_AS_OBJ(l->items[i]));
  }
}

// tpFree：释放 items 数组
static void listFree(MsObject* obj) {
  msFree(((MsListObj*)obj)->items);
}
```

### 4. VM 指令实现

```c
// OP_BUILD_LIST [2B: count]
case OP_BUILD_LIST: {
  uint16_t count = READ_U16();
  MsValue list = msNewList(count);
  MsListObj* l = (MsListObj*)MS_AS_OBJ(list);
  // 从栈上取 count 个元素（顺序：第一个元素在底部）
  t->sp -= count;
  for (uint16_t i = 0; i < count; i++) l->items[i] = t->sp[i];
  l->len = count;
  PUSH(list);
  DISPATCH();
}

// OP_UNPACK [1B: count]（解包到多个目标）
case OP_UNPACK: {
  uint8_t count = READ_BYTE();
  MsValue v = POP();
  MsListObj* l = (MsListObj*)MS_AS_OBJ(v);  // TODO: 支持任意可迭代对象
  if (l->len != count) return msValueError(t, "unpack mismatch");
  // 按相反顺序压栈（最后一个在栈顶，配合 SET_LOCAL 倒序）
  for (int i = (int)count - 1; i >= 0; i--) PUSH(l->items[i]);
  DISPATCH();
}
```

### 5. 类型槽

```c
static MsValue listLen(MsValue v) {
  return MS_INT_VAL(((MsListObj*)MS_AS_OBJ(v))->len);
}

static MsValue listEq(MsValue a, MsValue b) {
  if (!MS_IS_OBJ(b) || MS_AS_OBJ(b)->type != &msListType) return MS_BOOL_VAL(false);
  MsListObj* la = (MsListObj*)MS_AS_OBJ(a);
  MsListObj* lb = (MsListObj*)MS_AS_OBJ(b);
  if (la->len != lb->len) return MS_BOOL_VAL(false);
  for (uint32_t i = 0; i < la->len; i++) {
    if (!msValueEqual(la->items[i], lb->items[i])) return MS_BOOL_VAL(false);
  }
  return MS_BOOL_VAL(true);
}

static MsValue listGetItem(MsValue v, MsValue idx) {
  MsListObj* l = (MsListObj*)MS_AS_OBJ(v);
  if (!MS_IS_INT(idx)) return MS_ERROR_VALUE;
  return msListGet(l, MS_AS_INT(idx));
}

static MsValue listContains(MsValue v, MsValue item) {
  MsListObj* l = (MsListObj*)MS_AS_OBJ(v);
  for (uint32_t i = 0; i < l->len; i++) {
    if (msValueEqual(l->items[i], item)) return MS_BOOL_VAL(true);
  }
  return MS_BOOL_VAL(false);
}

MsType msListType = {
  .name = "list", .instanceSize = sizeof(MsListObj),
  .tpLen      = listLen,
  .tpEq       = listEq,
  .tpGetitem  = listGetItem,
  .tpSetitem  = listSetItem,
  .tpDelitem  = listDelItem,
  .tpIter     = listIter,
  .tpContains = listContains,
  .tpAdd      = listConcat,
  .tpMul      = listRepeat,
  .tpMark     = listMark,
  .tpFree     = listFree,
};
```

### 6. list 方法清单

| 方法 | 签名 | 说明 |
|---|---|---|
| `append(v)` | `(v: any) → nil` | 末尾追加 |
| `pop(i=-1)` | `(i: int) → any` | 移除并返回 |
| `insert(i, v)` | `(i: int, v: any) → nil` | 插入 |
| `remove(v)` | `(v: any) → nil` | 移除第一个匹配（不存在 → ValueError） |
| `index(v)` | `(v: any, start=0) → int` | 返回首次出现索引 |
| `count(v)` | `(v: any) → int` | 计数 |
| `sort(key=nil, reverse=false)` | 原地排序 | timsort（T099） |
| `reverse()` | `() → nil` | 原地反转 |
| `extend(iterable)` | 追加所有元素 | |
| `clear()` | `() → nil` | 清空 |
| `copy()` | `() → list` | 浅拷贝 |
| `join(sep)` | `(sep: str) → str` | 字符串列表拼接 |

---

## 验收标准（checklist）

- [ ] `[1, 2, 3][0]` → 1；`[1,2,3][-1]` → 3。
- [ ] `[1, 2] + [3, 4]` → `[1, 2, 3, 4]`。
- [ ] `[0] * 3` → `[0, 0, 0]`。
- [ ] `len([1,2,3])` → 3。
- [ ] `2 in [1,2,3]` → true；`5 not in [1,2,3]` → true。
- [ ] `[1,2,3].append(4)` → 列表变为 `[1,2,3,4]`。
- [ ] `[1,2,3].pop()` → 3，列表变为 `[1,2]`。
- [ ] `[3,1,2].sort()` → `[1,2,3]`。
- [ ] `[1,[2,3]].copy()` → 浅拷贝（内层 [2,3] 是同一对象）。
- [ ] GC：创建并丢弃 100 万个 list 后内存不泄漏。

---

## 测试用例（C 单测）

### `tests/vm/test_list.c`

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

static void testListBuild(void) {
  MsValue v = run("[1, 2, 3]");
  MS_ASSERT_TRUE(MS_IS_OBJ(v), "is obj");
  MsListObj* l = (MsListObj*)MS_AS_OBJ(v);
  MS_ASSERT_TRUE(l->len == 3, "len 3");
  MS_ASSERT_TRUE(MS_AS_INT(l->items[0]) == 1, "items[0]=1");
  MS_ASSERT_TRUE(MS_AS_INT(l->items[2]) == 3, "items[2]=3");
}

int main(void) {
  MS_RUN(testListBuild);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
// 构建与索引
nums := [1, 2, 3, 4, 5]
print(nums[0])      // 1
print(nums[-1])     // 5
print(nums[1:3])    // [2, 3]

// 修改
nums.append(6)
nums.insert(0, 0)
print(nums)         // [0, 1, 2, 3, 4, 5, 6]
nums.pop()
print(len(nums))    // 6

// 排序
words := ["banana", "apple", "cherry"]
words.sort()
print(words)        // ["apple", "banana", "cherry"]

// 推导式（T097 map/filter）
squares := list(map(func(x) { return x*x }, range(5)))
print(squares)      // [0, 1, 4, 9, 16]
```

---

## Benchmark

```ms
// benchmarks/bench_list.ms
n := 1_000_000
lst := []
for i in range(n) {
    lst.append(i)
}
print(len(lst))  // 1000000
// 目标：> 5M appends/sec

// 随机访问
for i in range(n) {
    _ = lst[i]
}
// 目标：> 50M indexed reads/sec
```

---

## 风险与边界

- **`items` 数组与 GC**：当 `msRealloc(l->items)` 被调用时，旧地址失效；GC 在 `tpMark` 时访问 `l->items`（当前有效地址），无问题。但在分配新元素前若触发 GC（`msGCAlloc` 内部），GC 可能扫描含旧指针的 `items`——需在 realloc 前禁止 GC 或用 `msGCPushRoot` 保护。初版简化：分配前保护根。
- **切片返回新 list**（而非视图）：初版不实现惰性切片视图；`[1,2,3][1:2]` 创建新 list。
- **sort 算法**：v1 使用 qsort（C 标准库），key 函数为 NULL 时按自然序；key 函数支持在 T099 实现。
