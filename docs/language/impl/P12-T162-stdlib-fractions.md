# P12-T162 stdlib: fractions

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `fractions` 模块（对齐 `stdlib/fractions.md`）：精确有理数，自动约分，支持与 int/float/Decimal 互操作。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T053 | int 算术（gcd） |
| P12-T138 | math.gcd |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-fractions.md` | §1 模块 API |

---

## API 清单

```ms
// 构造
fractions.Fraction(numerator=0, denominator=1)
fractions.Fraction("3/4")   // 从字符串
fractions.Fraction("0.125") // 从十进制字符串
fractions.Fraction(1.5)     // 从 float（精确：3/2）
fractions.Fraction(0.1)     // 3602879701896397/36028797018963968（精确！）

f := fractions.Fraction(3, 4)  // 3/4

// 算术（精确有理数）
f + g    f - g    f * g    f / g    f ** n
-f       abs(f)   int(f)   float(f)
f.numerator    // 3（约分后）
f.denominator  // 4（约分后，始终 > 0）

// 比较
f == g   f < g   f > g   f <= g   f >= g
f == 0.75   // true（与 float 精确比较）

// 限制分母（近似）
f.limit_denominator(max_denominator=1000000) → Fraction
// 找最接近 f 且分母 <= max_denominator 的有理数
// fractions.Fraction(3.14159265).limit_denominator(1000)
// → Fraction(355, 113)（π 的经典近似）

// 转换
str(f)   // "3/4"
repr(f)  // "Fraction(3, 4)"
```

---

## 实现要点

```c
// Fraction 内部：(numerator: int64_t, denominator: int64_t)
// 始终保持：gcd(|n|, d) = 1, d > 0
// 约分：gcd = math_gcd(abs(n), d); n /= gcd; d /= gcd
// 符号：分子携带符号，分母始终正

typedef struct MsFractionObj {
  MsObject header;
  int64_t  numer;    // 可负
  int64_t  denom;    // 始终正
} MsFractionObj;

// 溢出检测：n*d2 + n2*d 可能溢出 int64_t
// 若溢出：升级到 __int128 或 MsBigInt（T160 大整数）

// 加法：n1/d1 + n2/d2 = (n1*d2 + n2*d1) / (d1*d2)
// 先约分再计算（Stern-Brocot 或简化技巧）：
// g = gcd(d1,d2); t = n1*(d2/g) + n2*(d1/g); t/gcd(t,(d1/g)*d2)

// float → Fraction：使用 float.as_integer_ratio()
// 等同：n,d = float.as_integer_ratio(); Fraction(n,d)

// limit_denominator：Stern-Brocot 树搜索
// p0,q0=0,1; p1,q1=1,0; 二分直到 q1+q0 > max
```

---

## 验收标准（checklist）

- [ ] `Fraction(1,3) + Fraction(1,6)` → `Fraction(1,2)`（自动约分）。
- [ ] `Fraction(0.5)` → `Fraction(1,2)`（精确）。
- [ ] `Fraction("22/7")` → `Fraction(22,7)`。
- [ ] `Fraction(1,3) * 3` → `Fraction(1,1)`（= 1）。
- [ ] `Fraction(355,113).limit_denominator(100)` → `Fraction(22,7)`。
- [ ] `Fraction(1,3) < Fraction(1,2)` → `true`（精确比较）。

---

## 测试用例（.ms）

```ms
import fractions

// 基础
a := fractions.Fraction(1, 3)
b := fractions.Fraction(1, 6)
print(a + b)   // 1/2
print(a * b)   // 1/18
print(a / b)   // 2/1（=2）

// float 转换
print(fractions.Fraction(0.1))    // 3602879701896397/36028797018963968
print(fractions.Fraction(0.25))   // 1/4（精确）

// 字符串构造
print(fractions.Fraction("22/7"))  // 22/7

// limit_denominator（近似 π）
pi_approx := fractions.Fraction(3.14159265358979).limit_denominator(1000)
print(pi_approx)     // 355/113
print(float(pi_approx))  // 3.1415929...

// 混合算术
print(fractions.Fraction(1, 2) + 1)   // 3/2
print(fractions.Fraction(1, 2) > 0.4) // true（精确比较）
print(fractions.Fraction(1, 10) + fractions.Fraction(2, 10))
// 3/10（不是 0.30000000000000004）
```
