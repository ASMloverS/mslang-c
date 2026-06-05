# math — 数学函数与常量

```ms
import math
```

## 概述

提供常用数学函数和常量，语义对齐 Python `math` 模块。所有函数均操作
`float64`（IEEE 754 双精度）；整数辅助函数（`gcd`、`factorial` 等）接受
`int64` 参数。除非特别说明，传入 `NaN` 时返回 `NaN`，传入 `Inf` 时行为
与 IEEE 754 一致。

## 常量与类型

| 名称 | 值 | 说明 |
|---|---|---|
| `math.pi` | 3.141592653589793 | 圆周率 π |
| `math.e` | 2.718281828459045 | 自然常数 e |
| `math.tau` | 6.283185307179586 | 2π（一整圈弧度） |
| `math.inf` | +∞ | 正无穷大 float64 |
| `math.nan` | NaN | 非数值 float64 |

## 函数签名速查

**基本运算**

| 函数 | 签名 | 说明 |
|---|---|---|
| `sqrt` | `sqrt(x) → float` | 平方根 |
| `cbrt` | `cbrt(x) → float` | 立方根 |
| `pow` | `pow(x, y) → float` | x 的 y 次幂（浮点版，与内置 `pow` 不同） |
| `exp` | `exp(x) → float` | e^x |
| `exp2` | `exp2(x) → float` | 2^x |
| `expm1` | `expm1(x) → float` | e^x − 1，x 接近 0 时精度更高 |

**对数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `log` | `log(x, base=e) → float` | 对数；base 省略时为自然对数 |
| `log2` | `log2(x) → float` | 以 2 为底的对数 |
| `log10` | `log10(x) → float` | 以 10 为底的对数 |
| `log1p` | `log1p(x) → float` | log(1+x)，x 接近 0 时精度更高 |

**三角函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `sin` | `sin(x) → float` | 正弦（弧度） |
| `cos` | `cos(x) → float` | 余弦（弧度） |
| `tan` | `tan(x) → float` | 正切（弧度） |
| `asin` | `asin(x) → float` | 反正弦，返回 [−π/2, π/2] |
| `acos` | `acos(x) → float` | 反余弦，返回 [0, π] |
| `atan` | `atan(x) → float` | 反正切，返回 (−π/2, π/2) |
| `atan2` | `atan2(y, x) → float` | 四象限反正切，返回 (−π, π] |
| `degrees` | `degrees(r) → float` | 弧度转角度 |
| `radians` | `radians(d) → float` | 角度转弧度 |

**双曲函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `sinh` | `sinh(x) → float` | 双曲正弦 |
| `cosh` | `cosh(x) → float` | 双曲余弦 |
| `tanh` | `tanh(x) → float` | 双曲正切 |
| `asinh` | `asinh(x) → float` | 反双曲正弦 |
| `acosh` | `acosh(x) → float` | 反双曲余弦，要求 x ≥ 1 |
| `atanh` | `atanh(x) → float` | 反双曲正切，要求 −1 < x < 1 |

**取整与符号**

| 函数 | 签名 | 说明 |
|---|---|---|
| `ceil` | `ceil(x) → int` | 向上取整，返回 int |
| `floor` | `floor(x) → int` | 向下取整，返回 int |
| `trunc` | `trunc(x) → int` | 向零截断，返回 int |
| `abs` | `abs(x) → float` | 绝对值（浮点） |
| `fabs` | `fabs(x) → float` | 同 abs，强调返回 float |
| `copysign` | `copysign(x, y) → float` | 取 x 的量级，符号来自 y |

**整数辅助**

| 函数 | 签名 | 说明 |
|---|---|---|
| `gcd` | `gcd(*integers) → int` | 最大公因数；无参返回 0 |
| `lcm` | `lcm(*integers) → int` | 最小公倍数；无参返回 1 |
| `factorial` | `factorial(n) → int` | n!；n < 0 时抛 ValueError |
| `comb` | `comb(n, k) → int` | 组合数 C(n,k)；k > n 时返回 0 |
| `perm` | `perm(n, k=nil) → int` | 排列数 P(n,k)；k=nil 时 k=n |
| `isqrt` | `isqrt(n) → int` | 整数平方根（向下取整），要求 n ≥ 0 |

**浮点信息**

| 函数 | 签名 | 说明 |
|---|---|---|
| `frexp` | `frexp(x) → (float, int)` | 返回 (mantissa, exp)，满足 x = mantissa × 2^exp，0.5 ≤ \|mantissa\| < 1 |
| `ldexp` | `ldexp(mantissa, exp) → float` | mantissa × 2^exp |
| `modf` | `modf(x) → (float, float)` | 返回 (小数部分, 整数部分)，两者符号与 x 相同 |
| `fmod` | `fmod(x, y) → float` | 浮点取余，符号与 x 相同 |
| `remainder` | `remainder(x, y) → float` | IEEE 754 余数，结果绝对值不超过 y/2 |
| `ulp` | `ulp(x) → float` | x 处的最小精度单位（unit in the last place） |

**特殊值检测**

| 函数 | 签名 | 说明 |
|---|---|---|
| `isnan` | `isnan(x) → bool` | x 是否为 NaN |
| `isinf` | `isinf(x) → bool` | x 是否为 ±∞ |
| `isfinite` | `isfinite(x) → bool` | x 是否既非 NaN 也非 ±∞ |
| `isclose` | `isclose(a, b, relTol=1e-9, absTol=0.0) → bool` | 近似相等判断 |

**特殊函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `hypot` | `hypot(*coords) → float` | 欧氏范数 √(x₁²+x₂²+…) |
| `dist` | `dist(p, q) → float` | 两点间欧氏距离，p/q 为坐标元组 |
| `erf` | `erf(x) → float` | 误差函数 |
| `erfc` | `erfc(x) → float` | 互补误差函数（= 1 − erf(x)） |
| `gamma` | `gamma(x) → float` | Gamma 函数 |
| `lgamma` | `lgamma(x) → float` | Gamma 函数的自然对数的绝对值 |

**数乘**

| 函数 | 签名 | 说明 |
|---|---|---|
| `prod` | `prod(iter, start=1) → number` | 对可迭代对象求积 |
| `fsum` | `fsum(iter) → float` | 精确浮点求和（补偿算法），避免累积误差 |

## 详细语义

### math.atan2(y, x)

注意参数顺序：**y 在前，x 在后**。返回点 (x, y) 相对原点的方位角，范围
(−π, π]。`atan2(0, -1)` 返回 π，`atan2(0, 1)` 返回 0。

### math.log(x, base=e)

```ms
math.log(math.e)       // 1.0，自然对数
math.log(100, 10)      // 2.0，以 10 为底
math.log(8, 2)         // 3.0，以 2 为底
```

`base` 省略时等同于 `ln(x)`。x ≤ 0 时抛 `ValueError`。

### math.isclose(a, b, relTol=1e-9, absTol=0.0)

判定条件：`|a−b| ≤ max(relTol × max(|a|, |b|), absTol)`。

- `relTol`：相对容差，用于量级相近的比较。
- `absTol`：绝对容差，用于接近零的比较（`relTol` 在零附近失效）。

两个 `inf` 被视为相等；`NaN` 与任何值（包括自身）均不相等。

### math.hypot(*coords)

接受任意维度坐标：

```ms
math.hypot(3.0, 4.0)           // 5.0（二维）
math.hypot(1.0, 1.0, 1.0)      // √3（三维）
```

### math.fsum(iter)

使用 Shewchuk 算法对 float64 序列求精确和，避免普通累加的舍入误差累积。
对于大量数值相加或量级差异大的序列，推荐使用此函数代替内置 `sum`。

## 示例

```ms
import math

// 勾股定理
func hypotenuse(a, b) {
    return math.sqrt(a*a + b*b)
}

// 角度转弧度并求正弦
func sinDegrees(deg) {
    return math.sin(math.radians(deg))
}

// 精确浮点求和
vals := [0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1]
fmt.println(math.fsum(vals))   // 1.0（而非 0.9999...）

// 近似相等
fmt.println(math.isclose(0.1 + 0.2, 0.3))        // true
fmt.println(math.isclose(1e10 + 1, 1e10))         // false（超出 relTol）
fmt.println(math.isclose(1e-10, 0.0, absTol=1e-9)) // true

// 整数辅助
fmt.println(math.gcd(48, 36))      // 12
fmt.println(math.lcm(4, 6))        // 12
fmt.println(math.factorial(10))    // 3628800
fmt.println(math.comb(10, 3))      // 120
fmt.println(math.isqrt(17))        // 4
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | 参数域错误，如 `sqrt(-1)`、`log(0)`、`factorial(-1)`、`asin(2)` |
| `OverflowError` | 结果超出 float64 范围，如 `math.factorial` 的结果超出 int64 |
| `ZeroDivisionError` | `fmod(x, 0)` 等除零场景 |
