# P8-T103 内置函数：set / frozenset / bytes / bytearray 构造

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现容器类型的内置构造函数：`set(iterable)`、`frozenset(iterable)`、`bytes(source)`、`bytearray(source)`，使其可从任意可迭代对象构造。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T058 | MsBytesObj |
| P4-T062 | MsSetObj |
| P4-T063 | MsFrozensetObj |
| P4-T065 | 迭代协议 |

---

## 实现要点

### 1. `set(iterable=())`

```c
// set() → 空集合
// set([1,2,1]) → {1,2}
// set("abc") → {"a","b","c"}
static MsValue builtin_set(MsThread* t, MsValue* args, int argc) {
    MsValue s = msNewSet();
    if (argc == 0) return s;
    MsValue iter = msGetIter(t, args[0]);
    if (MS_IS_ERROR(iter)) return iter;
    MsType* ty = msTypeOf(iter);
    while (true) {
        MsValue v = ty->tp_next(iter);
        if (MS_IS_NIL(v)) break;
        if (MS_IS_ERROR(v)) return v;
        MsValue r = msSetAdd(s, v);
        if (MS_IS_ERROR(r)) return r;
    }
    return s;
}
```

### 2. `frozenset(iterable=())`

```c
static MsValue builtin_frozenset(MsThread* t, MsValue* args, int argc) {
    // 先收集为动态数组，再一次性构建 frozenset
    if (argc == 0) return msNewFrozenset(NULL, 0);

    // 临时集合去重
    MsValue tmp = builtin_set(t, args, argc);
    if (MS_IS_ERROR(tmp)) return tmp;
    MsSetObj* so = (MsSetObj*)MS_AS_OBJ(tmp);

    // 提取所有元素
    MsValue* items = msAllocTmp(so->count * sizeof(MsValue));
    uint32_t n = 0;
    for (uint32_t i = 0; i < so->cap; i++) {
        if (!MS_IS_NIL(so->entries[i].key) && !MS_IS_ERROR(so->entries[i].key))
            items[n++] = so->entries[i].key;
    }
    MsValue fs = msNewFrozenset(items, n);
    msFreeTmp(items);
    return fs;
}
```

### 3. `bytes(source)` / `bytes(n)` / `bytes(iterable)`

```c
static MsValue builtin_bytes(MsThread* t, MsValue* args, int argc) {
    if (argc == 0) return msNewBytes(NULL, 0);
    MsValue x = args[0];

    // bytes(n) → n 个零字节
    if (MS_IS_INT(x)) {
        int64_t n = MS_AS_INT(x);
        if (n < 0) return msRaiseValueError(t, "bytes() argument must be non-negative");
        uint8_t* buf = msAlloc((size_t)n);
        memset(buf, 0, (size_t)n);
        MsValue v = msNewBytesNoCopy(buf, (uint32_t)n);
        return v;
    }

    // bytes("utf-8 str") → 已移至 str.encode()；这里只允许整数或可迭代
    // bytes([65,66,67]) → b"ABC"
    MsValue iter = msGetIter(t, x);
    if (MS_IS_ERROR(iter)) return iter;
    MsWriter buf = {0};
    MsType* ty = msTypeOf(iter);
    while (true) {
        MsValue v = ty->tp_next(iter);
        if (MS_IS_NIL(v)) break;
        if (!MS_IS_INT(v) || MS_AS_INT(v) < 0 || MS_AS_INT(v) > 255)
            return msRaiseValueError(t, "bytes() elements must be int in 0-255");
        writerWriteByte(&buf, (uint8_t)MS_AS_INT(v));
    }
    return msNewBytesNoCopy(buf.data, buf.len);
}
```

### 4. `bytearray`

```c
// bytearray 与 bytes 类似，但返回可变的 MsBytesObj（T058 已实现可变性）
// bytearray 只是别名或 subtype，内部用同一 MsBytesObj
```

---

## 验收标准（checklist）

- [ ] `set([1,2,1,3])` → `{1,2,3}`（去重）。
- [ ] `frozenset([1,2,3])` → 可哈希。
- [ ] `bytes(3)` → `b"\x00\x00\x00"`。
- [ ] `bytes([65,66,67])` → `b"ABC"`。
- [ ] `bytes([256])` → `ValueError`（超出 0-255）。
- [ ] `set("hello")` → `{"h","e","l","o"}`（字符集合）。

---

## 测试用例（.ms）

```ms
// set
s := set([1,2,2,3,3,3])
print(len(s))  // 3
print(3 in s)  // true

// frozenset
fs := frozenset([1,2,3])
print(fs)          // frozenset({1, 2, 3})
m := {fs: "key"}   // frozenset 可做 map 键
print(m[fs])       // key

// bytes
b := bytes(5)
print(b)     // b'\x00\x00\x00\x00\x00'
b2 := bytes([72,101,108,108,111])
print(b2)    // b'Hello'

// 错误
try { bytes([-1]) } catch ValueError as e { print(e.message) }
```

---

## Benchmark

```ms
// benchmarks/bench_set_build.ms
n := 100_000
lst := list(range(n))
t0 := time.now()
s := set(lst)
t1 := time.now()
print("set from 100K list:", t1-t0, "ms")
// 目标 < 50ms
```

---

## 风险与边界

- **`bytearray` vs `bytes`**：初版两者使用同一底层类型（`MsBytesObj`），仅类型名不同；后续可分离为 immutable `bytes` 和 mutable `bytearray`。
