# P8-T098 内置函数：range / enumerate / zip / map / filter（惰性）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现惰性迭代器工厂函数：`enumerate`、`zip`、`map`、`filter`（`range` 在 P4-T064 已完成）。这些函数返回迭代器对象，不立即求值，支持 `for x in ...` 语法和嵌套组合。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T064 | range（已完成） |
| P4-T065 | 迭代协议 |

---

## 实现要点

### 1. `enumerate`

```c
// MsEnumerateObj: {iter, idx}
typedef struct MsEnumerateObj {
  MsObject header;
  MsValue  iter;    // 底层迭代器
  int64_t  idx;     // 当前索引（从 start 开始）
} MsEnumerateObj;

MsType msEnumerateType = {
  .name = "enumerate",
  .tpIter  = enumerateSelf,
  .tpNext  = enumerateNext,  // → (idx, val) tuple
  .tpMark  = enumerateMark,
};

static MsValue enumerateNext(MsValue v) {
  MsEnumerateObj* e = (MsEnumerateObj*)MS_AS_OBJ(v);
  MsType* iterTy = msTypeOf(e->iter);
  MsValue val = iterTy->tpNext(e->iter);
  if (MS_IS_NIL(val)) return MS_NIL_VAL;  // exhausted
  // 构建 (idx, val) 元组
  MsValue items[2] = { MS_INT_VAL(e->idx++), val };
  return msNewTuple(items, 2);
}

// builtin enumerate(iterable, start=0)
static MsValue builtinEnumerate(MsThread* t, MsValue* args, int argc) {
  if (argc < 1) return msRaiseTypeError(t, "enumerate requires iterable");
  int64_t start = (argc >= 2) ? MS_AS_INT(args[1]) : 0;
  MsValue iter = msGetIter(t, args[0]);
  if (MS_IS_ERROR(iter)) return iter;
  MsEnumerateObj* e = msGCAlloc(sizeof(*e), &msEnumerateType);
  e->iter = iter;
  e->idx  = start;
  return MS_OBJ_VAL((MsObject*)e);
}
```

### 2. `zip`

```c
// MsZipObj: {iters[], count}  → 每次 next 从各 iter 各取一个，打包为 tuple
typedef struct MsZipObj {
  MsObject header;
  MsValue* iters;
  uint32_t count;
} MsZipObj;

static MsValue zipNext(MsValue v) {
  MsZipObj* z = (MsZipObj*)MS_AS_OBJ(v);
  MsValue* items = msAllocTmp(z->count * sizeof(*items));
  for (uint32_t i = 0; i < z->count; i++) {
    MsType* ty = msTypeOf(z->iters[i]);
    MsValue val = ty->tpNext(z->iters[i]);
    if (MS_IS_NIL(val)) { msFreeTmp(items); return MS_NIL_VAL; }
    items[i] = val;
  }
  MsValue tup = msNewTuple(items, z->count);
  msFreeTmp(items);
  return tup;
}
```

### 3. `map`

```c
// MsMapIterObj: {func, iter}
typedef struct MsMapIterObj {
  MsObject header;
  MsValue  func;
  MsValue  iter;
} MsMapIterObj;

static MsValue mapNext(MsValue v) {
  MsMapIterObj* m = (MsMapIterObj*)MS_AS_OBJ(v);
  MsType* ty = msTypeOf(m->iter);
  MsValue val = ty->tpNext(m->iter);
  if (MS_IS_NIL(val)) return MS_NIL_VAL;
  MsValue arg = val;
  return msCallFn(gThread, m->func, &arg, 1);
}
```

### 4. `filter`

```c
// MsFilterObj: {func（可为 nil→identity）, iter}
static MsValue filterNext(MsValue v) {
  MsFilterObj* f = (MsFilterObj*)MS_AS_OBJ(v);
  MsType* ty = msTypeOf(f->iter);
  while (true) {
    MsValue val = ty->tpNext(f->iter);
    if (MS_IS_NIL(val)) return MS_NIL_VAL;

    bool keep;
    if (MS_IS_NIL(f->func)) {
      keep = msValueTruthy(val);
    } else {
      MsValue r = msCallFn(gThread, f->func, &val, 1);
      if (MS_IS_ERROR(r)) return r;
      keep = msValueTruthy(r);
    }
    if (keep) return val;
  }
}
```

---

## 验收标准（checklist）

- [ ] `enumerate([a,b,c], start=1)` → `(1,a) (2,b) (3,c)`。
- [ ] `zip([1,2],[3,4])` → `(1,3) (2,4)`；其中一个耗尽则停止。
- [ ] `zip([], [1,2])` → 空。
- [ ] `map(str, [1,2,3])` → `"1" "2" "3"`。
- [ ] `filter(None, [0, 1, "", "x"])` → `1 "x"`。
- [ ] 惰性：`zip` 中 `next` 调用次数 = 元素数（不预取）。

---

## 测试用例（.ms）

```ms
// enumerate
for i, v in enumerate(["a","b","c"]) {
    print(i, v)
}
// 0 a / 1 b / 2 c

// zip
for a, b in zip([1,2,3], [10,20,30]) {
    print(a + b)
}
// 11 / 22 / 33

// zip 不等长
print(list(zip([1,2,3], [10,20])))  // [(1,10),(2,20)]

// map
doubled := list(map(func(x) { return x * 2 }, [1,2,3,4]))
print(doubled)  // [2, 4, 6, 8]

// filter
evens := list(filter(func(x) { return x % 2 == 0 }, range(10)))
print(evens)    // [0, 2, 4, 6, 8]

// 组合
result := list(map(str, filter(func(x){ return x>3 }, range(6))))
print(result)   // ["4", "5"]
```

---

## Benchmark

```ms
// 惰性 vs 预求值
n := 10_000_000
sum := 0
for x in map(func(i){ return i*2 }, range(n)) { sum = sum + x }
print(sum)  // 目标 < 3s
```

---

## 风险与边界

- **`map` 接受多个 iterable**：Python 中 `map(f, iter1, iter2)` 相当于 `map(f, zip(iter1, iter2))`；初版只支持单个 iterable，多个 iterable 暂不支持（文档说明）。
