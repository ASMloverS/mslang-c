# P8-T099 内置函数：sorted / reversed / sum / min / max

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现聚合/排序内置函数：`sorted`（返回新列表）、`reversed`（惰性反转迭代器）、`sum`、`min`、`max`。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T059 | list（sort / msListCopy） |
| P4-T065 | 迭代协议 |

---

## 实现要点

### 1. `sorted(iterable, key=None, reverse=False)`

```c
static MsValue builtinSorted(MsThread* t, MsValue* args, int argc) {
  // 1. 收集 iterable 到 list
  MsValue lst = msCollectToList(t, args[0]);
  if (MS_IS_ERROR(lst)) return lst;

  // 2. 提取 key / reverse 关键字参数（T070 之后完整支持）
  MsValue key     = (argc >= 2) ? args[1] : MS_NIL_VAL;
  bool    reverse = (argc >= 3) ? msValueTruthy(args[2]) : false;

  // 3. 调用 list.sort(key, reverse)（复用 T059 的排序）
  MsListObj* lo = (MsListObj*)MS_AS_OBJ(lst);
  int r = msListSort(t, lo, key, reverse);
  if (r != 0) return MS_ERROR_VALUE;
  return lst;
}
```

### 2. `reversed`（惰性）

```c
// MsReversedObj: {seq（list/tuple/str）, idx}
// 若 seq 有 __reversed__ 则调用；否则要求有 __len__ + __getitem__
typedef struct MsReversedObj {
  MsObject header;
  MsValue  seq;
  int64_t  idx;   // 从 len-1 开始递减
} MsReversedObj;

static MsValue reversedNext(MsValue v) {
  MsReversedObj* r = (MsReversedObj*)MS_AS_OBJ(v);
  if (r->idx < 0) return MS_NIL_VAL;
  MsType* ty = msTypeOf(r->seq);
  MsValue item = ty->tpGetitem(r->seq, MS_INT_VAL(r->idx--));
  return item;
}

static MsValue builtinReversed(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "reversed() takes 1 argument");
  MsValue seq = args[0];
  MsType* ty  = msTypeOf(seq);
  if (!ty->tpLen || !ty->tpGetitem)
    return msRaiseTypeError(t, "argument to reversed() must be a sequence");
  int64_t n = ty->tpLen(seq);
  MsReversedObj* r = msGCAlloc(sizeof(*r), &msReversedType);
  r->seq = seq;
  r->idx = n - 1;
  return MS_OBJ_VAL((MsObject*)r);
}
```

### 3. `sum(iterable, start=0)`

```c
static MsValue builtinSum(MsThread* t, MsValue* args, int argc) {
  if (argc < 1) return msRaiseTypeError(t, "sum() requires iterable");
  MsValue acc = (argc >= 2) ? args[1] : MS_INT_VAL(0);

  MsValue iter = msGetIter(t, args[0]);
  if (MS_IS_ERROR(iter)) return iter;
  MsType* ty = msTypeOf(iter);
  while (true) {
    MsValue val = ty->tpNext(iter);
    if (MS_IS_NIL(val)) break;
    MsValue newAcc = msValueAdd(t, acc, val);  // 调用 tpAdd
    if (MS_IS_ERROR(newAcc)) return newAcc;
    acc = newAcc;
  }
  return acc;
}
```

### 4. `min` / `max`

```c
// min(iterable, key=None) / min(a, b, c, ..., key=None)
static MsValue builtinMinMax(MsThread* t, MsValue* args, int argc, bool isMax) {
  if (argc == 0) return msRaiseTypeError(t, "min/max require at least 1 argument");

  MsValue key = MS_NIL_VAL;  // TODO: 支持 key= 关键字参数

  MsValue iter;
  if (argc == 1) {
    iter = msGetIter(t, args[0]);
  } else {
    // 多参数：把 args 当成 iterable
    MsValue lst = msNewList();
    for (int i = 0; i < argc; i++) msListAppend(lst, args[i]);
    iter = msGetIter(t, lst);
  }
  if (MS_IS_ERROR(iter)) return iter;

  MsType* ty = msTypeOf(iter);
  MsValue best = MS_NIL_VAL;  // "无值" 标记
  while (true) {
    MsValue v = ty->tpNext(iter);
    if (MS_IS_NIL(v)) break;
    if (MS_IS_NIL(best)) { best = v; continue; }
    // 比较
    bool betterThanBest;
    if (isMax) betterThanBest = msValueLt(t, best, v);  // best < v → v 更大
    else       betterThanBest = msValueLt(t, v, best);  // v < best → v 更小
    if (MS_IS_ERROR(MS_BOOL_VAL(betterThanBest))) return MS_ERROR_VALUE;
    if (betterThanBest) best = v;
  }
  if (MS_IS_NIL(best)) return msRaiseValueError(t, "min/max() arg is an empty sequence");
  return best;
}
```

---

## 验收标准（checklist）

- [ ] `sorted([3,1,2])` → `[1,2,3]`（新列表，原列表不变）。
- [ ] `sorted([3,1,2], reverse=True)` → `[3,2,1]`。
- [ ] `sorted(["b","a","c"], key=len)` → 按长度排序。
- [ ] `list(reversed([1,2,3]))` → `[3,2,1]`。
- [ ] `sum([1,2,3,4])` → `10`；`sum([], 100)` → `100`。
- [ ] `min([3,1,2])` → `1`；`max(5,3,9,2)` → `9`。

---

## 测试用例（.ms）

```ms
// sorted
s := sorted([3,1,4,1,5,9])
print(s)   // [1, 1, 3, 4, 5, 9]

words := ["banana", "apple", "cherry"]
print(sorted(words))                          // ["apple", "banana", "cherry"]
print(sorted(words, key=func(w){ return len(w) }))  // ["apple", "banana", "cherry"]

// reversed
print(list(reversed([1,2,3])))  // [3, 2, 1]

// sum
print(sum(range(101)))   // 5050
print(sum([1.5, 2.5]))   // 4.0

// min / max
print(min(3,1,2))           // 1
print(max([10, 20, 30]))    // 30
print(min([], 0) ?? "?")    // ValueError（空序列）
```

---

## Benchmark

```ms
// benchmarks/bench_sorted.ms
import random
lst := list(range(100_000))
// random.shuffle(lst)
t0 := time.now()
s := sorted(lst)
t1 := time.now()
print("sorted 100K:", t1 - t0, "ms")
// 目标 < 200ms（Timsort）
```

---

## 风险与边界

- **排序稳定性**：`msListSort` 必须为稳定排序（Tim sort 或 merge sort）；`key` 函数可能抛出异常，须在比较循环中传播 MS_ERROR_VALUE。
