# P4-T065 迭代协议（GET_ITER / FOR_ITER / 切片指令）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现通用迭代协议的 VM 层：`OP_GET_ITER`（将 iterable 转为 iterator）、`OP_FOR_ITER`（取下一个元素或跳出循环），以及 `OP_BUILD_SLICE`（构建切片对象供 `OP_GET_ITEM` 使用）。这些指令将所有实现了 `tpIter`/`tpNext` 类型槽的对象统一接入 `for` 循环。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T059 | list 类型对象（`tpIter`/`tpNext` 由本任务实现，`ms_list.c` 注明 deferred to T065） |
| P4-T060 | map 类型对象（迭代器由本任务实现） |
| P4-T061 | tuple 类型对象（迭代器由本任务实现） |
| P4-T064 | range iter（已实现，`tpIter`/`tpNext` 带 `struct MsVM* vm` 首参，本任务须与之保持签名一致） |
| P4-T057 | str 类型对象（迭代器由本任务实现，`ms_str.c` 注明 deferred to T065） |
| P4-T051 | 求值循环 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §3.10 迭代（GET_ITER / FOR_ITER）、§9 opcode 命名映射 |
| `type-system.md` | §4 迭代器协议、§1.3 MsType 类型槽（tpIter / tpNext） |

---

## 待实现（C 文件）

```
src/vm/ms_vm.c              # OP_GET_ITER / OP_FOR_ITER / OP_BUILD_SLICE case
src/runtime/ms_slice.c      # MsSliceObj + msNewSlice / msSliceNormalize（新增）
include/mslang/ms_slice.h   # MsSliceObj 公开声明（新增）
src/runtime/ms_list.c       # MsListIterObj + listIterNext/listIterTraverse + msListType.tpIter/tpNext + list 切片
include/mslang/ms_list.h    # MsListIterObj / msListIterType 声明
src/runtime/ms_map.c        # MsMapIterObj + mapIterNext/mapIterTraverse + msMapType.tpIter/tpNext
include/mslang/ms_map.h     # MsMapIterObj / msMapIterType 声明
src/runtime/ms_str.c        # str 迭代器 + msStrType.tpIter/tpNext + str 切片
src/runtime/ms_tuple.c      # tuple 迭代器 + msTupleType.tpIter/tpNext + tuple 切片
src/runtime/ms_set.c        # msSetType.tpIter/tpNext
src/runtime/ms_frozenset.c  # msFrozensetType.tpIter/tpNext
```

---

## 实现要点

### 1. StopIteration 约定

mslang 使用 **`MS_NIL_VAL`** 表示迭代耗尽（`tpNext` 返回 nil），而非抛出异常（避免异常传播开销）。这是内部约定；外部 `next()` 内置函数在 nil 时抛出 `StopIteration`（T101）。

**已知限制（v1）**：该哨兵与合法的 `nil` 元素/键无法区分——`list`/`map` 均允许 `nil` 作为元素或键（`type-system.md §2.7/§2.8`），因此 `for x in [1, nil, 3]` 或 `for k in {nil: 1}` 会在遇到 `nil` 时被误判为耗尽而提前退出循环。`range` 迭代器不受影响（元素恒为 int）。v1 不解决此问题；如需支持含 nil 容器的正确迭代，需引入独立于 `MS_NIL_VAL`/`MS_ERROR_VALUE` 的专用哨兵（留待后续任务评估，不在本任务范围内）。

### 2. `OP_GET_ITER`

```c
case OP_GET_ITER: {
  MsValue iterable = POP();
  struct MsType* tp = msTypeOf(iterable);
  if (!tp->tpIter) {
    return msTypeError(t, "'%s' object is not iterable", tp->name);
  }
  MsValue iter = tp->tpIter(&gVM, iterable);
  if (MS_IS_ERROR(iter)) return iter;
  PUSH(iter);
  DISPATCH();
}
```

### 3. `OP_FOR_ITER`

```c
// OP_FOR_ITER [3B: exit_offset (signed AX)]
// 栈顶：迭代器对象（不弹出）
// 若有下一个值：压入值，继续
// 若耗尽（tpNext 返回 nil）：弹出迭代器，跳转 exit_offset
case OP_FOR_ITER: {
  int32_t offset = READ_JUMP_OFFSET();
  MsValue iter = PEEK(0);
  struct MsType* tp = msTypeOf(iter);
  if (!tp->tpNext) {
    return msTypeError(t, "not an iterator");
  }
  MsValue val = tp->tpNext(&gVM, iter);
  if (MS_IS_NIL(val)) {
    // 耗尽：弹出迭代器，跳转
    (void)POP();
    frame->ip += offset;
  } else {
    // 有值：压入元素（迭代器留在栈上，下一轮 FOR_ITER 使用）
    PUSH(val);
  }
  DISPATCH();
}
```

### 4. 各类型迭代器实现

**list 迭代器**（`MsListIterObj`）：

```c
struct MsListIterObj {
  struct MsObject head;
  MsValue         list;   // 被迭代的 list（存为 MsValue，供 traverse 就地改写，同 ms_list.c/ms_set.c 约定）
  uint32_t        idx;
};

static MsValue listIterNext(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsListIterObj* it = (struct MsListIterObj*) MS_AS_OBJ(v);
  struct MsListObj* list = (struct MsListObj*) MS_AS_OBJ(it->list);
  if (it->idx >= list->len) return MS_NIL_VAL;
  return list->items[it->idx++];
}

// traverse：访问 list 引用槽，供移动 GC 就地改写（同 listTraverse/msSetTraverse 约定）
static void listIterTraverse(struct MsObject* obj, MsVisitFn visit, void* ctx) {
  struct MsListIterObj* it = (struct MsListIterObj*) obj;
  visit(&it->list, ctx);
}
```

**map 迭代器**（`MsMapIterObj`）：迭代键（for k in map），通过 `items()` 可迭代键值对。

```c
// for k in map → 迭代键（按哈希槽顺序，不保证顺序，同 type-system.md §4）
// for k, v in map.items() → 迭代键值对 tuple
struct MsMapIterObj {
  struct MsObject  head;
  struct MsMapObj* map;
  uint32_t         slotIdx;  // 当前探测的 entries 下标
};

static MsValue mapIterNext(struct MsVM* vm, MsValue v) {
  (void) vm;
  struct MsMapIterObj* it = (struct MsMapIterObj*) MS_AS_OBJ(v);
  while (it->slotIdx < it->map->cap) {
    struct MsMapEntry* e = &it->map->entries[it->slotIdx++];
    if (e->occupied) {
      return e->key;  // 返回键（occupied 区分真实条目与空槽/墓碑，nil 键同样有效）
    }
  }
  return MS_NIL_VAL;
}
```

### 5. `OP_BUILD_SLICE`

```c
// 编译器对 obj[lo:hi:step] 生成（vm.md §3.8: BUILD_SLICE | A: flags）：
//   compileExpr(obj)
//   若 lo 存在则 compileExpr(lo)      （省略则不压栈）
//   若 hi 存在则 compileExpr(hi)      （省略则不压栈）
//   若 step 存在则 compileExpr(step)  （省略则不压栈）
//   OP_BUILD_SLICE flags   // flags: bit0=has_start, bit1=has_stop, bit2=has_step
//   OP_GET_ITEM

// OP_BUILD_SLICE：按 flags 位仅弹出实际压栈的分量（弹出顺序与压栈相反：先 step 后 hi 再 lo），
// 缺省分量填 nil，构建 MsSliceObj 压栈。
case OP_BUILD_SLICE: {
  uint8_t flags = READ_BYTE();
  MsValue step = (flags & 0x4) ? POP() : MS_NIL_VAL;
  MsValue hi = (flags & 0x2) ? POP() : MS_NIL_VAL;
  MsValue lo = (flags & 0x1) ? POP() : MS_NIL_VAL;
  MsValue slice = msNewSlice(lo, hi, step);
  PUSH(slice);
  DISPATCH();
}
```

**MsSliceObj**：

```c
struct MsSliceObj {
  struct MsObject head;
  MsValue         start;
  MsValue         stop;
  MsValue         step;
};

// 规范化（将 nil 替换为默认值，支持负索引）
void msSliceNormalize(struct MsSliceObj* s, int64_t len,
                      int64_t* outStart, int64_t* outStop, int64_t* outStep);
```

`OP_GET_ITEM` 检测栈顶 key 是否为 `MsSliceObj`；若是，调用类型的切片处理（list/str/bytes/tuple 各自实现）。

---

## 验收标准（checklist）

- [ ] `for i in range(5) { }` → 迭代 0-4（使用 GET_ITER + FOR_ITER）。
- [ ] `for x in [1,2,3] { }` → 迭代 1,2,3。
- [ ] `for c in "hello" { }` → 迭代单个字符（str iter）。
- [ ] `for k in {"a":1, "b":2} { }` → 迭代出全部键（顺序为哈希槽顺序，不保证等于插入顺序）。
- [ ] `[1,2,3][1:2]` → `[2]`（list 切片）。
- [ ] `"hello"[1:4]` → `"ell"`（str 切片）。
- [ ] `(1,2,3)[::2]` → `(1,3)`（tuple 步长切片）。
- [ ] `"hello"[-1:]` → `"o"`（负索引切片）。
- [ ] 不可迭代类型 → TypeError。
- [ ] `OP_FOR_ITER` 在迭代器耗尽时正确弹出并跳转。

---

## 测试用例（C 单测）

### `tests/vm/test_iteration.c`

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

static void testForRange(void) {
  MsValue v = run("s := 0\nfor i in range(5) { s += i }\ns");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 10, "sum=10");
}

static void testListSlice(void) {
  MsValue v = run("len([1,2,3,4,5][1:4])");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 3, "slice len=3");
}

static void testStrSlice(void) {
  MsValue v = run("\"hello\"[1:3]");
  MsStrObj* s = (MsStrObj*)MS_AS_OBJ(v);
  MS_ASSERT_TRUE(s->len == 2 && s->data[0] == 'e', "str slice");
}

int main(void) {
  MS_RUN(testForRange);
  MS_RUN(testListSlice);
  MS_RUN(testStrSlice);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
// for 循环各类型
for c in "hello" {
    print(c, end=" ")   // h e l l o
}

for k, v in {"a": 1, "b": 2}.items() {
    print($"{k}={v}")   // a=1 b=2
}

// 切片
nums := [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
print(nums[2:7])       // [2, 3, 4, 5, 6]
print(nums[::2])       // [0, 2, 4, 6, 8]
print(nums[::-1])      // [9, 8, 7, 6, 5, 4, 3, 2, 1, 0]

// 字符串切片
s := "Hello, World!"
print(s[7:12])    // World
print(s[-6:-1])   // World

// 自定义可迭代对象（需 T072 class + __iter__/__next__）
```

---

## Benchmark

N/A（迭代协议性能归入 T064 range bench）。

---

## 风险与边界

- **`tpNext` 返回 nil 作为 StopIteration 哨兵**：这是内部约定，与 `list`/`map` 中合法的 `nil` 元素/键存在冲突——`for x in [1, nil, 3]` 会在 `nil` 处被误判为耗尽而提前退出（见实现要点 §1「已知限制」）。v1 接受此限制，不做规避。
- **map 迭代顺序一致性**：迭代中修改 map（添加/删除键）→ 未定义行为（v1 不检测，类 Python 规则：不要在迭代中修改容器大小）。
- **切片规范化**：负索引、越界索引的处理逻辑（截断至 [0, len]）集中在 `msSliceNormalize` 中，各类型切片实现调用此函数，避免重复。
