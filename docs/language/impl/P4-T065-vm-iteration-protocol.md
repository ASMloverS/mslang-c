# P4-T065 迭代协议（GET_ITER / FOR_ITER / 切片指令）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现通用迭代协议的 VM 层：`OP_GET_ITER`（将 iterable 转为 iterator）、`OP_FOR_ITER`（取下一个元素或跳出循环），以及 `OP_BUILD_SLICE`（构建切片对象供 `OP_GET_INDEX` 使用）。这些指令将所有实现了 `tp_iter`/`tp_next` 类型槽的对象统一接入 `for` 循环。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T059 | list（`tp_iter`/`tp_next`） |
| P4-T060 | map iter |
| P4-T061 | tuple iter |
| P4-T064 | range iter |
| P4-T057 | str iter |
| P4-T051 | 求值循环 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `vm.md` | §2 FOR_ITER / GET_ITER 指令语义 |
| `type-system.md` | §16 迭代协议（tp_iter / tp_next / StopIteration） |

---

## 待实现（C 文件）

```
src/vm/ms_vm.c   # OP_GET_ITER / OP_FOR_ITER / OP_BUILD_SLICE case
```

---

## 实现要点

### 1. StopIteration 约定

mslang 使用 **`MS_NIL_VAL`** 表示迭代耗尽（`tp_next` 返回 nil），而非抛出异常（避免异常传播开销）。这是内部约定；外部 `next()` 内置函数在 nil 时抛出 `StopIteration`（T101）。

### 2. `OP_GET_ITER`

```c
case OP_GET_ITER: {
    MsValue iterable = POP();
    MsType* tp = msTypeOf(iterable);
    if (!tp->tp_iter) {
        return msTypeError(t, "'%s' object is not iterable", tp->name);
    }
    MsValue iter = tp->tp_iter(iterable);
    if (MS_IS_ERROR(iter)) return iter;
    PUSH(iter);
    DISPATCH();
}
```

### 3. `OP_FOR_ITER`

```c
// OP_FOR_ITER [2B: exit_offset]
// 栈顶：迭代器对象（不弹出）
// 若有下一个值：压入值，继续
// 若耗尽（tp_next 返回 nil）：弹出迭代器，跳转 exit_offset
case OP_FOR_ITER: {
    uint16_t offset = READ_U16();
    MsValue iter = PEEK(0);
    MsType* tp = msTypeOf(iter);
    if (!tp->tp_next) {
        return msTypeError(t, "not an iterator");
    }
    MsValue val = tp->tp_next(iter);
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
typedef struct MsListIterObj {
    MsObject  header;
    MsListObj* list;   // 被迭代的 list（GC 持有引用）
    uint32_t  idx;
} MsListIterObj;

static MsValue listIterNext(MsValue v) {
    MsListIterObj* it = (MsListIterObj*)MS_AS_OBJ(v);
    if (it->idx >= it->list->len) return MS_NIL_VAL;
    return it->list->items[it->idx++];
}

// tp_mark：标记 list 引用
static void listIterMark(MsObject* obj) {
    markObject((MsObject*)((MsListIterObj*)obj)->list);
}
```

**map 迭代器**（`MsMapIterObj`）：迭代键（for k in map），通过 `items()` 可迭代键值对。

```c
// for k in map → 迭代键（按插入顺序）
// for k, v in map.items() → 迭代键值对 tuple
typedef struct MsMapIterObj {
    MsObject  header;
    MsMapObj* map;
    uint32_t  slotIdx;  // 当前探测的 entries 下标
} MsMapIterObj;

static MsValue mapIterNext(MsValue v) {
    MsMapIterObj* it = (MsMapIterObj*)MS_AS_OBJ(v);
    while (it->slotIdx < it->map->cap) {
        MsMapEntry* e = &it->map->entries[it->slotIdx++];
        if (!MS_IS_NIL(e->key) && !MS_IS_ERROR(e->key)) {
            return e->key;  // 返回键
        }
    }
    return MS_NIL_VAL;
}
```

### 5. `OP_BUILD_SLICE`

```c
// 编译器对 obj[lo:hi:step] 生成：
//   compileExpr(obj)
//   compileExpr(lo)  (若省略 → OP_NIL)
//   compileExpr(hi)  (若省略 → OP_NIL)
//   compileExpr(step)(若省略 → OP_NIL)
//   OP_BUILD_SLICE
//   OP_GET_INDEX

// OP_BUILD_SLICE：弹出 step/hi/lo，构建 MsSliceObj，压栈
case OP_BUILD_SLICE: {
    MsValue step = POP(), hi = POP(), lo = POP();
    MsValue slice = msNewSlice(lo, hi, step);
    PUSH(slice);
    DISPATCH();
}
```

**MsSliceObj**：

```c
typedef struct MsSliceObj {
    MsObject  header;
    MsValue   start;
    MsValue   stop;
    MsValue   step;
} MsSliceObj;

// 规范化（将 nil 替换为默认值，支持负索引）
void msSliceNormalize(MsSliceObj* s, int64_t len,
                      int64_t* outStart, int64_t* outStop, int64_t* outStep);
```

`OP_GET_INDEX` 检测栈顶 key 是否为 `MsSliceObj`；若是，调用类型的切片处理（list/str/bytes/tuple 各自实现）。

---

## 验收标准（checklist）

- [ ] `for i in range(5) { }` → 迭代 0-4（使用 GET_ITER + FOR_ITER）。
- [ ] `for x in [1,2,3] { }` → 迭代 1,2,3。
- [ ] `for c in "hello" { }` → 迭代单个字符（str iter）。
- [ ] `for k in {"a":1, "b":2} { }` → 迭代键（"a","b"）。
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

- **`tp_next` 返回 nil 作为 StopIteration 哨兵**：这是内部约定，不影响 mslang 代码中 `nil` 值的正常迭代（list 中的 nil 元素通过 iter 内部计数管理，不会被误识别为耗尽）。
- **map 迭代顺序一致性**：迭代中修改 map（添加/删除键）→ 未定义行为（v1 不检测，类 Python 规则：不要在迭代中修改容器大小）。
- **切片规范化**：负索引、越界索引的处理逻辑（截断至 [0, len]）集中在 `msSliceNormalize` 中，各类型切片实现调用此函数，避免重复。
