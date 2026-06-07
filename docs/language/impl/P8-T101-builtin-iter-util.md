# P8-T101 内置函数：any / all / iter / next / callable / hash / id

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现迭代与对象检测类内置函数：`any`、`all`（短路求值）、`iter`（获取迭代器）、`next`（手动推进迭代器）、`callable`、`hash`、`id`。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T065 | 迭代协议 |
| P5-T077 | callable 判断 |

---

## 实现要点

### 1. `any` / `all`

```c
// any(iterable) → true if any element is truthy（短路）
static MsValue builtinAny(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "any() takes 1 argument");
  MsValue iter = msGetIter(t, args[0]);
  if (MS_IS_ERROR(iter)) return iter;
  MsType* ty = msTypeOf(iter);
  while (true) {
    MsValue v = ty->tpNext(iter);
    if (MS_IS_NIL(v)) return MS_BOOL_VAL(false);
    if (MS_IS_ERROR(v)) return v;
    if (msValueTruthy(v)) return MS_BOOL_VAL(true);
  }
}

// all(iterable) → true if all elements are truthy（短路）
static MsValue builtinAll(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "all() takes 1 argument");
  MsValue iter = msGetIter(t, args[0]);
  if (MS_IS_ERROR(iter)) return iter;
  MsType* ty = msTypeOf(iter);
  while (true) {
    MsValue v = ty->tpNext(iter);
    if (MS_IS_NIL(v)) return MS_BOOL_VAL(true);
    if (MS_IS_ERROR(v)) return v;
    if (!msValueTruthy(v)) return MS_BOOL_VAL(false);
  }
}
```

### 2. `iter` / `next`

```c
// iter(obj) → 迭代器；iter(callable, sentinel) → 调用直到等于 sentinel
static MsValue builtinIter(MsThread* t, MsValue* args, int argc) {
  if (argc == 1) return msGetIter(t, args[0]);
  if (argc == 2) {
    // callable sentinel 形式
    MsValue callable = args[0], sentinel = args[1];
    return msNewCallableSentinelIter(callable, sentinel);
  }
  return msRaiseTypeError(t, "iter() takes 1 or 2 arguments");
}

// next(iterator, default=<none>)
static MsValue builtinNext(MsThread* t, MsValue* args, int argc) {
  if (argc < 1) return msRaiseTypeError(t, "next() requires argument");
  MsValue iter = args[0];
  MsType* ty   = msTypeOf(iter);
  if (!ty->tpNext)
    return msRaiseTypeError(t, "argument is not an iterator");
  MsValue v = ty->tpNext(iter);
  if (MS_IS_NIL(v)) {
    if (argc >= 2) return args[1];  // 返回 default
    return msRaiseStopIteration(t);
  }
  return v;
}
```

### 3. `callable`

```c
static MsValue builtinCallable(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "callable() takes 1 argument");
  MsValue v = args[0];
  MsType* ty = msTypeOf(v);
  bool ok = (ty->tpCall != NULL);
  if (!ok && MS_IS_OBJ(v) && MS_AS_OBJ(v)->type == &msInstanceType) {
    // 检查 __call__ 魔术方法
    ok = !MS_IS_NIL(msTypeLookupMethodMRO(
                 ((MsInstanceObj*)MS_AS_OBJ(v))->klass, "__call__"));
  }
  return MS_BOOL_VAL(ok);
}
```

### 4. `hash`

```c
static MsValue builtinHash(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "hash() takes 1 argument");
  MsType* ty = msTypeOf(args[0]);
  if (!ty->tpHash) return msRaiseTypeError(t, "unhashable type");
  uint32_t h = ty->tpHash(args[0]);
  return MS_INT_VAL((int64_t)(int32_t)h);  // 转 signed 与 Python 一致
}
```

### 5. `id`

```c
static MsValue builtinId(MsThread* t, MsValue* args, int argc) {
  if (argc != 1) return msRaiseTypeError(t, "id() takes 1 argument");
  MsValue v = args[0];
  // 标量（int/float/bool/nil）：返回值本身的位模式（不稳定，仅供调试）
  if (MS_IS_OBJ(v)) return MS_INT_VAL((int64_t)(uintptr_t)MS_AS_OBJ(v));
  // 按 MsValue 的位模式作 id（简化版）
  uint64_t bits;
  memcpy(&bits, &v, sizeof(bits));
  return MS_INT_VAL((int64_t)bits);
}
```

---

## 验收标准（checklist）

- [ ] `any([0, "", false, 1])` → `true`（短路在第 4 个元素）。
- [ ] `all([1, true, "x"])` → `true`；`all([1, 0])` → `false`。
- [ ] `next(iter([1,2,3]))` → `1`，再次 → `2`。
- [ ] `next(iter([]), "done")` → `"done"`（有默认值）。
- [ ] `next(iter([]))` → `StopIteration`（无默认值）。
- [ ] `callable(print)` → `true`；`callable(42)` → `false`。
- [ ] `hash(42) == hash(42.0)` → `true`（int/float 同值同哈希）。
- [ ] `id(x) == id(x)` → `true`（同对象）。

---

## 测试用例（.ms）

```ms
// any / all
print(any([0, 0, 0, 1]))    // true
print(all([1, 2, 3]))        // true
print(all([1, 0, 3]))        // false

// iter / next
it := iter([10, 20, 30])
print(next(it))   // 10
print(next(it))   // 20

try { next(iter([])) } catch StopIteration { print("stopped") }  // stopped

print(next(iter([]), "def"))  // def

// callable
func f() {}
print(callable(f))     // true
print(callable(42))    // false
print(callable(print)) // true

// hash
print(hash(42) == hash(42.0))   // true
print(hash("hello") == hash("hello"))  // true

// id
x := [1,2,3]
print(id(x) == id(x))   // true
y := [1,2,3]
print(id(x) == id(y))   // false（不同对象）
```

---

## Benchmark

N/A（轻量工具函数）。

---

## 风险与边界

- **`id` 的 GC 移动**：当前 STW 标记清除 GC 不移动对象，`id()` 在对象生命周期内稳定；分代 GC（P10）引入 Cheney 复制后，`id()` 在 GC 后可能改变（与 Python 一致，文档说明）。
