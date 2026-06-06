# P8-T100 内置函数：abs / round / pow / divmod

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现数学相关内置函数：`abs`（绝对值）、`round`（四舍五入）、`pow`（幂，支持三参数模运算）、`divmod`（同时返回商和余数）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T053 | int 算术（整数 pow / divmod） |
| P4-T054 | float 算术 |

---

## 实现要点

### 1. `abs(x)`

```c
static MsValue builtin_abs(MsThread* t, MsValue* args, int argc) {
    if (argc != 1) return msRaiseTypeError(t, "abs() takes 1 argument");
    MsValue x = args[0];
    if (MS_IS_INT(x)) {
        int64_t v = MS_AS_INT(x);
        if (v == INT64_MIN) return msRaiseOverflowError(t, "abs(INT64_MIN) overflow");
        return MS_INT_VAL(v < 0 ? -v : v);
    }
    if (MS_IS_FLOAT(x)) {
        return MS_FLOAT_VAL(fabs(MS_AS_FLOAT(x)));
    }
    if (MS_IS_BOOL(x)) return MS_INT_VAL(MS_AS_BOOL(x) ? 1 : 0);
    // __abs__ dunder
    return msCallDunder(t, x, "__abs__", NULL, 0);
}
```

### 2. `round(number, ndigits=0)`

```c
// round(x)    → 四舍五入到最近整数（banker's rounding）
// round(x, n) → 保留 n 位小数
static MsValue builtin_round(MsThread* t, MsValue* args, int argc) {
    if (argc < 1) return msRaiseTypeError(t, "round() requires argument");
    MsValue x = args[0];
    int ndigits = (argc >= 2) ? (int)MS_AS_INT(args[1]) : 0;

    if (MS_IS_INT(x)) {
        if (ndigits >= 0) return x;  // 整数已是精确值
        // ndigits < 0：四舍五入到 10^|ndigits| 位
        int64_t factor = 1;
        for (int i = 0; i < -ndigits; i++) factor *= 10;
        int64_t v = MS_AS_INT(x);
        int64_t half = factor / 2;
        int64_t rem  = ((v % factor) + factor) % factor;
        // Banker's rounding
        if (rem < half || (rem == half && ((v / factor) % 2 == 0)))
            return MS_INT_VAL(v - (v % factor < 0 ? v % factor + factor : v % factor));
        else
            return MS_INT_VAL(v + factor - rem);
    }
    if (MS_IS_FLOAT(x)) {
        double d = MS_AS_FLOAT(x);
        double factor = pow(10.0, ndigits);
        return MS_FLOAT_VAL(round(d * factor) / factor);
    }
    return msCallDunder(t, x, "__round__", &args[1], argc - 1);
}
```

### 3. `pow(base, exp, mod=None)`

```c
// pow(2, 10) → 1024 (int)
// pow(2, -1) → 0.5 (float)
// pow(2, 10, 1000) → 24 (modular exponentiation, int only)
static MsValue builtin_pow(MsThread* t, MsValue* args, int argc) {
    if (argc < 2) return msRaiseTypeError(t, "pow() requires at least 2 arguments");
    MsValue base = args[0], exp_ = args[1];
    bool hasMod = (argc >= 3 && !MS_IS_NIL(args[2]));

    if (hasMod) {
        // 三参数：全整数，模幂
        if (!MS_IS_INT(base) || !MS_IS_INT(exp_) || !MS_IS_INT(args[2]))
            return msRaiseTypeError(t, "pow() with 3 args requires int arguments");
        int64_t b = MS_AS_INT(base), e = MS_AS_INT(exp_), m = MS_AS_INT(args[2]);
        if (e < 0) return msRaiseValueError(t, "pow() 3rd argument prohibits exp < 0");
        if (m == 0) return msRaiseValueError(t, "pow() 3rd argument cannot be 0");
        return MS_INT_VAL(msPowMod(b, e, m));  // fast modular exp
    }

    // 两参数：复用 tp_pow（T053/T054）
    MsType* ty = msTypeOf(base);
    if (ty->tp_pow) return ty->tp_pow(base, exp_);
    return msRaiseTypeError(t, "unsupported operand type for pow()");
}
```

### 4. `divmod(a, b)`

```c
// divmod(a, b) → (a // b, a % b)（同一操作保证一致性）
static MsValue builtin_divmod(MsThread* t, MsValue* args, int argc) {
    if (argc != 2) return msRaiseTypeError(t, "divmod() takes 2 arguments");
    MsValue a = args[0], b = args[1];

    if (MS_IS_INT(a) && MS_IS_INT(b)) {
        int64_t bi = MS_AS_INT(b);
        if (bi == 0) return msRaiseZeroDivisionError(t);
        int64_t ai = MS_AS_INT(a);
        int64_t q = ai / bi, r = ai % bi;
        // 向负无穷取整（与 Python floordiv 一致）
        if ((r != 0) && ((r < 0) != (bi < 0))) { q--; r += bi; }
        MsValue items[2] = { MS_INT_VAL(q), MS_INT_VAL(r) };
        return msNewTuple(items, 2);
    }
    // float
    double af = msToFloat(a), bf = msToFloat(b);
    double q = floor(af / bf), r = fmod(af, bf);
    if ((r != 0.0) && ((r < 0) != (bf < 0))) { q -= 1.0; r += bf; }
    MsValue items[2] = { MS_FLOAT_VAL(q), MS_FLOAT_VAL(r) };
    return msNewTuple(items, 2);
}
```

---

## 验收标准（checklist）

- [ ] `abs(-5)` → `5`；`abs(-3.14)` → `3.14`；`abs(INT64_MIN)` → `OverflowError`。
- [ ] `round(3.5)` → `4`；`round(2.5)` → `2`（banker's rounding）。
- [ ] `round(1234, -2)` → `1200`。
- [ ] `pow(2, 10)` → `1024`；`pow(2, -1)` → `0.5`。
- [ ] `pow(2, 10, 1000)` → `24`（模幂）。
- [ ] `divmod(17, 5)` → `(3, 2)`；`divmod(-17, 5)` → `(-4, 3)`。

---

## 测试用例（.ms）

```ms
// abs
print(abs(-42))      // 42
print(abs(-3.14))    // 3.14

// round
print(round(3.14159, 2))  // 3.14
print(round(2.5))         // 2  (banker's)
print(round(3.5))         // 4
print(round(1234, -2))    // 1200

// pow
print(pow(2, 10))         // 1024
print(pow(2, -3))         // 0.125
print(pow(3, 4, 100))     // 81

// divmod
q, r := divmod(17, 5)
print(q, r)    // 3 2

q, r = divmod(-17, 5)
print(q, r)    // -4 3
```

---

## Benchmark

N/A（内置数学函数性能由 C 实现决定，不需要 .ms benchmark）。

---

## 风险与边界

- **`round` 的 banker's rounding**：Python 用 banker's rounding（四舍六入五取偶）；C 标准库 `round()` 是四舍五入（远离零）；需手动实现。
