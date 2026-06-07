# P12-T138 stdlib: math（常量 + 基础函数）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `math` 模块的常量和基础数学函数（对应 C 标准库 `<math.h>`），包括：数学常量（π、e、∞、nan）、幂/对数、取整、绝对值、最大公因数等。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T054 | float 类型 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-math-basic.md` | §1 模块 API |

---

## API 清单

```ms
// 常量（对齐 stdlib/math.md）
math.pi     // 3.141592653589793
math.e      // 2.718281828459045
math.tau    // 6.283185307179586
math.inf    // +∞ (float("inf"))
math.nan    // NaN
math.phi    // 黄金比例 1.6180339887...

// 基础函数
math.sqrt(x)       → float
math.cbrt(x)       → float（立方根）
math.pow(x, y)     → float（与 builtin pow() 不同：总返回 float）
math.exp(x)        → float（e^x）
math.exp2(x)       → float（2^x）
math.log(x, base=e) → float
math.log2(x)       → float
math.log10(x)      → float
math.log1p(x)      → float（log(1+x)，精度更好）

// 取整
math.ceil(x)       → int
math.floor(x)      → int
math.trunc(x)      → int（向零截断）
math.round(x, n=0) → 同 builtin round（Banker's rounding）

// 绝对值/符号
math.fabs(x)       → float（总返回 float，与 abs() 的区别）
math.copysign(x,y) → float（x 的大小，y 的符号）
math.sign(x)       → int（-1/0/1）

// 整数数论
math.gcd(*args)    → int（最大公因数，支持多参数）
math.lcm(*args)    → int（最小公倍数）
math.comb(n, k)    → int（组合数 C(n,k)）
math.perm(n, k=None) → int（排列数 P(n,k)）
math.factorial(n)  → int
math.isqrt(n)      → int（整数平方根，下取整）

// 判断
math.isfinite(x)   → bool
math.isinf(x)      → bool
math.isnan(x)      → bool
math.isclose(a,b,rel_tol=1e-9,abs_tol=0.0) → bool

// 其他
math.fsum(iterable)   → float（精确浮点求和，避免累积误差）
math.prod(iterable, start=1) → 积
math.hypot(*coords)   → float（欧氏距离 √(x²+y²+...)）
math.dist(p, q)       → float（两点距离）
math.ldexp(x, i)      → float（x * 2^i）
math.frexp(x)         → (m, e)（分解为尾数+指数）
math.modf(x)          → (frac, int_part)
```

---

## 实现要点

```c
// 绝大多数函数直接包装 <math.h>：sqrt/exp/log/ceil/floor 等
// gcd: 辗转相除法（支持负数和多参数）
// comb(n,k): 利用 gcd 避免大整数溢出
// fsum: Neumaier 求和算法

static MsValue mathGCD(MsThread* t, MsValue* args, int argc) {
  if (argc == 0) return MS_INT_VAL(0);
  int64_t g = llabs(MS_AS_INT(args[0]));
  for (int i = 1; i < argc; i++) {
    int64_t b = llabs(MS_AS_INT(args[i]));
    while (b) { int64_t tmp = b; b = g % b; g = tmp; }
  }
  return MS_INT_VAL(g);
}
```

---

## 验收标准（checklist）

- [ ] `math.pi` ≈ 3.14159265358979（至少 15 位精度）。
- [ ] `math.sqrt(2)` ≈ 1.41421356（与 `**0.5` 一致）。
- [ ] `math.gcd(12, 8, 4)` → `4`。
- [ ] `math.factorial(10)` → `3628800`。
- [ ] `math.isclose(0.1+0.2, 0.3)` → `true`。
- [ ] `math.fsum([0.1]*10)` → `1.0`（精确，无浮点误差）。

---

## 测试用例（.ms）

```ms
import math

print(math.pi)          // 3.141592653589793
print(math.sqrt(2))     // 1.4142135623730951
print(math.log(math.e)) // 1.0
print(math.ceil(1.1))   // 2
print(math.floor(1.9))  // 1
print(math.gcd(48, 18)) // 6
print(math.factorial(10))  // 3628800
print(math.comb(10, 3))    // 120
print(math.isnan(math.nan))   // true
print(math.isinf(math.inf))   // true
print(math.isclose(0.1+0.2, 0.3))  // true
print(math.fsum([0.1]*10))    // 1.0
```

---

## Benchmark

```ms
// benchmarks/bench_math.ms
import math, time
n := 1_000_000
t0 := time.now()
for i in range(n) { math.sqrt(i) }
t1 := time.now()
print("1M sqrt:", t1-t0, "ms")  // 目标 < 200ms
```

---

## 风险与边界

- **`math.comb(n, k)` 大整数**：当 n、k 很大时（如 C(100,50)），需要大整数支持（P12-T158 decimal 或 int bignum 扩展）；初版限制结果 < INT64_MAX，超出时抛 `OverflowError`。
