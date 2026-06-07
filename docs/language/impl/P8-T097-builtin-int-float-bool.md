# P8-T097 内置函数：int / float / bool / 数值转换

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现数值构造/转换内置函数：`int()`、`float()`、`bool()`，支持从字符串、浮点、布尔等类型转换。同时实现 `complex()`（基础，P12 decimal 之前的占位）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T053 | int 类型 |
| P4-T054 | float 类型 |
| P4-T055 | bool / nil 类型 |

---

## 实现要点

### 1. `int(x, base=10)`

```c
static MsValue builtinInt(MsThread* t, MsValue* args, int argc) {
  if (argc == 0) return MS_INT_VAL(0);

  MsValue x = args[0];
  int base = (argc >= 2) ? (int)MS_AS_INT(args[1]) : 10;

  if (MS_IS_INT(x))   return x;
  if (MS_IS_BOOL(x))  return MS_INT_VAL(MS_AS_BOOL(x) ? 1 : 0);
  if (MS_IS_FLOAT(x)) {
    double d = MS_AS_FLOAT(x);
    if (d != d) return msRaiseValueError(t, "int() cannot convert nan");
    if (d > (double)INT64_MAX || d < (double)INT64_MIN)
      return msRaiseOverflowError(t, "int() result too large");
    return MS_INT_VAL((int64_t)d);
  }
  if (MS_IS_OBJ(x) && MS_AS_OBJ(x)->type == &msStrType) {
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(x);
    // 去前后空格后解析
    char* endptr;
    long long val = strtoll(s->data, &endptr, base);
    // 跳过尾部空格
    while (isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0')
      return msRaiseValueError(t, "invalid literal for int()");
    return MS_INT_VAL((int64_t)val);
  }
  // __int__ dunder
  MsValue result = msCallDunder(t, x, "__int__", NULL, 0);
  if (!MS_IS_NIL(result)) return result;
  return msRaiseTypeError(t, "int() argument must be str, bytes or number");
}
```

### 2. `float(x)`

```c
static MsValue builtinFloat(MsThread* t, MsValue* args, int argc) {
  if (argc == 0) return MS_FLOAT_VAL(0.0);
  MsValue x = args[0];
  if (MS_IS_FLOAT(x))  return x;
  if (MS_IS_INT(x))    return MS_FLOAT_VAL((double)MS_AS_INT(x));
  if (MS_IS_BOOL(x))   return MS_FLOAT_VAL(MS_AS_BOOL(x) ? 1.0 : 0.0);
  if (MS_IS_OBJ(x) && MS_AS_OBJ(x)->type == &msStrType) {
    MsStrObj* s = (MsStrObj*)MS_AS_OBJ(x);
    char* endptr;
    double val = strtod(s->data, &endptr);
    while (isspace((unsigned char)*endptr)) endptr++;
    if (*endptr != '\0')
      return msRaiseValueError(t, "invalid literal for float()");
    return MS_FLOAT_VAL(val);
  }
  // __float__ dunder
  MsValue result = msCallDunder(t, x, "__float__", NULL, 0);
  if (!MS_IS_NIL(result)) return result;
  return msRaiseTypeError(t, "float() argument must be str or number");
}
```

### 3. `bool(x)`

```c
static MsValue builtinBool(MsThread* t, MsValue* args, int argc) {
  if (argc == 0) return MS_BOOL_VAL(false);
  return MS_BOOL_VAL(msValueTruthy(args[0]));
}
```

---

## 验收标准（checklist）

- [ ] `int("42")` → `42`；`int("0xFF", 16)` → `255`；`int("0o17", 8)` → `15`。
- [ ] `int(3.9)` → `3`（截断，不四舍五入）。
- [ ] `int("abc")` → `ValueError`。
- [ ] `float("3.14")` → `3.14`；`float("inf")` → `+∞`；`float("nan")` → `nan`。
- [ ] `bool(0)` → `false`；`bool([])` → `false`；`bool([1])` → `true`。
- [ ] `int("  42  ")` → `42`（忽略前后空格）。

---

## 测试用例（.ms）

```ms
// int 转换
print(int(3.9))         // 3
print(int("42"))        // 42
print(int("0xff", 16))  // 255
print(int("0b1010", 2)) // 10
print(int(true))        // 1
print(int(false))       // 0

// float 转换
print(float("3.14"))    // 3.14
print(float(42))        // 42.0
print(float("inf"))     // inf

// bool 转换
print(bool(0))          // false
print(bool(1))          // true
print(bool(""))         // false
print(bool("x"))        // true
print(bool([]))         // false
print(bool([0]))        // true

// 错误
try { int("abc") } catch ValueError as e { print(e.message) }
// invalid literal for int()
```

---

## Benchmark

```ms
// benchmarks/bench_int_conv.ms
n := 1_000_000
for i in range(n) { int(3.14) }
// 目标 < 500ms（含循环开销）
```

---

## 风险与边界

- **`int()` 无参**：返回 `0`（与 Python 一致）。
- **`int(float("nan"))`**：`nan` 转 int → `ValueError`（IEEE 754 nan 无整数表示）。
- **自定义类 `__int__`**：调用 dunder 方法后需验证返回值确实是 int 类型，否则 `TypeError`。
