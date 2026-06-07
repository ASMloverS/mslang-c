# P4-T064 range 迭代器

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `range` 内置函数与 `MsRangeObj` 运行时类型：惰性整数范围迭代器（不预分配整数列表），支持 `range(stop)`/`range(start, stop)`/`range(start, stop, step)` 三种形式，以及正反向迭代、成员检查（O(1)）、len（O(1)）、索引（O(1)）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T053 | int 类型（range 元素为 int） |
| P4-T050 | `msGCAlloc` |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `type-system.md` | §15 range 类型 |
| `stdlib/itertools.md` | range 相关（参考） |

---

## 待实现（C 文件）

```
src/runtime/ms_range.c     # MsRangeObj + MsRangeIterObj
include/mslang/ms_range.h  # msNewRange / msBuiltinRange
```

---

## 实现要点

### 1. MsRangeObj 结构

```c
typedef struct MsRangeObj {
  MsObject  header;
  int64_t   start;
  int64_t   stop;
  int64_t   step;    // 非零，可正可负
} MsRangeObj;

// range 迭代器（独立对象，不修改 MsRangeObj）
typedef struct MsRangeIterObj {
  MsObject  header;
  int64_t   cur;    // 当前值
  int64_t   stop;
  int64_t   step;
} MsRangeIterObj;
```

### 2. 构造

```c
// range(stop)
// range(start, stop)
// range(start, stop, step)
MsValue msBuiltinRange(MsValue* args, int argc) {
  int64_t start = 0, stop, step = 1;
  if (argc == 1) {
    if (!MS_IS_INT(args[0])) return MS_ERROR_VALUE;
    stop = MS_AS_INT(args[0]);
  } else if (argc == 2) {
    if (!MS_IS_INT(args[0]) || !MS_IS_INT(args[1])) return MS_ERROR_VALUE;
    start = MS_AS_INT(args[0]); stop = MS_AS_INT(args[1]);
  } else if (argc == 3) {
    if (!MS_IS_INT(args[0]) || !MS_IS_INT(args[1]) || !MS_IS_INT(args[2]))
      return MS_ERROR_VALUE;
    start = MS_AS_INT(args[0]); stop = MS_AS_INT(args[1]);
    step  = MS_AS_INT(args[2]);
    if (step == 0) return MS_ERROR_VALUE;  // ValueError
  } else {
    return MS_ERROR_VALUE;  // TypeError
  }
  return msNewRange(start, stop, step);
}
```

### 3. 类型槽

```c
// len(range)：O(1)
static MsValue rangeLen(MsValue v) {
  MsRangeObj* r = (MsRangeObj*)MS_AS_OBJ(v);
  if ((r->step > 0 && r->stop <= r->start) ||
    (r->step < 0 && r->stop >= r->start)) {
    return MS_INT_VAL(0);
  }
  int64_t n = (r->stop - r->start + r->step - (r->step > 0 ? 1 : -1)) / r->step;
  return MS_INT_VAL(n > 0 ? n : 0);
}

// range[i]：O(1)
static MsValue rangeGetItem(MsValue v, MsValue idx) {
  MsRangeObj* r = (MsRangeObj*)MS_AS_OBJ(v);
  if (!MS_IS_INT(idx)) return MS_ERROR_VALUE;
  int64_t len = MS_AS_INT(rangeLen(v));
  int64_t i = MS_AS_INT(idx);
  if (i < 0) i += len;
  if (i < 0 || i >= len) return MS_ERROR_VALUE;  // IndexError
  return MS_INT_VAL(r->start + i * r->step);
}

// x in range：O(1)
static MsValue rangeContains(MsValue v, MsValue item) {
  MsRangeObj* r = (MsRangeObj*)MS_AS_OBJ(v);
  if (!MS_IS_INT(item)) return MS_BOOL_VAL(false);
  int64_t x = MS_AS_INT(item);
  if (r->step > 0) {
    if (x < r->start || x >= r->stop) return MS_BOOL_VAL(false);
    return MS_BOOL_VAL((x - r->start) % r->step == 0);
  } else {
    if (x > r->start || x <= r->stop) return MS_BOOL_VAL(false);
    return MS_BOOL_VAL((r->start - x) % (-r->step) == 0);
  }
}

// iter(range) → MsRangeIterObj
static MsValue rangeIter(MsValue v) {
  MsRangeObj* r = (MsRangeObj*)MS_AS_OBJ(v);
  MsRangeIterObj* it = (MsRangeIterObj*)msGCAlloc(&msRangeIterType, sizeof(*it));
  it->cur  = r->start;
  it->stop = r->stop;
  it->step = r->step;
  return MS_OBJ_VAL(it);
}

// next(range_iter) → int 或 nil（耗尽）
static MsValue rangeIterNext(MsValue v) {
  MsRangeIterObj* it = (MsRangeIterObj*)MS_AS_OBJ(v);
  if ((it->step > 0 && it->cur >= it->stop) ||
    (it->step < 0 && it->cur <= it->stop)) {
    return MS_NIL_VAL;  // StopIteration 用 nil 标记（T065 协议）
  }
  MsValue result = MS_INT_VAL(it->cur);
  it->cur += it->step;
  return result;
}

MsType msRangeType = {
  .name = "range", .instanceSize = sizeof(MsRangeObj),
  .tpLen      = rangeLen,
  .tpGetitem  = rangeGetItem,
  .tpContains = rangeContains,
  .tpIter     = rangeIter,
  .tpEq       = rangeEq,
  .tpRepr     = rangeRepr,
  .tpMark     = NULL,  // 只含 int 字段，无 GC 子对象
};
```

---

## 验收标准（checklist）

- [ ] `range(5)` → 迭代产生 0,1,2,3,4。
- [ ] `range(2, 8, 2)` → 迭代产生 2,4,6。
- [ ] `range(5, 0, -1)` → 迭代产生 5,4,3,2,1。
- [ ] `len(range(10))` → 10（O(1)）。
- [ ] `range(10)[3]` → 3（O(1)）。
- [ ] `5 in range(10)` → true；`10 in range(10)` → false（O(1)）。
- [ ] `range(0)` → 空序列（len=0）。
- [ ] `range(0, 0, -1)` → 空（len=0）。
- [ ] `for i in range(3) { print(i) }` → 0 1 2。
- [ ] `range(0, 10, 0)` → ValueError（step=0）。

---

## 测试用例（C 单测）

### `tests/vm/test_range.c`

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

static void testRangeLen(void) {
  MsValue v = run("len(range(10))");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 10, "len=10");
}

static void testRangeIn(void) {
  MsValue v = run("5 in range(10)");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && MS_AS_BOOL(v), "5 in range(10)");
  v = run("10 in range(10)");
  MS_ASSERT_TRUE(MS_IS_BOOL(v) && !MS_AS_BOOL(v), "10 not in range(10)");
}

static void testRangeFor(void) {
  MsValue v = run("s := 0\nfor i in range(5) { s += i }\ns");
  MS_ASSERT_TRUE(MS_IS_INT(v) && MS_AS_INT(v) == 10, "sum=10");
}

int main(void) {
  MS_RUN(testRangeLen);
  MS_RUN(testRangeIn);
  MS_RUN(testRangeFor);
  return msTestSummary();
}
```

### .ms 使用示例

```ms
// 基本 range
for i in range(5) {
    print(i)     // 0 1 2 3 4
}

// 步长
for i in range(0, 10, 2) {
    print(i)     // 0 2 4 6 8
}

// 倒序
for i in range(10, 0, -1) {
    print(i)     // 10 9 8 ... 1
}

// O(1) 操作
r := range(1_000_000)
print(len(r))           // 1000000
print(r[999999])        // 999999
print(500000 in r)      // true（不扫描列表）

// 转为列表
first10 := list(range(10))
print(first10)  // [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
```

---

## Benchmark

```ms
// benchmarks/bench_range.ms
// for 循环 range（核心热点）
n := 100_000_000
sum := 0
for i in range(n) {
    sum += i
}
print(sum)
// 目标：> 100M iterations/sec（range 迭代是最常见的循环模式）
```

---

## 风险与边界

- **`range` 是内置函数**（T096 完整注册），本任务先将 `msBuiltinRange` 直接注册到全局命名空间（`msVMInit` 中）。
- **`range` 与 `list(range(n))`**：转换时分配 n 个 int 值的 list，n 大时消耗内存；用户应使用 `for i in range(n)` 而非 `list(range(n))`（文档提示）。
- **`MsRangeIterObj` 是独立 GC 对象**：创建 iter 时引用 `MsRangeObj`（通过值复制 start/stop/step，无需持有引用）→ 不需要 `tpMark`。
