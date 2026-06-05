# fractions — 有理数

```ms
import fractions
```

## 概述

精确的有理数（分子/分母）表示与运算，避免浮点误差。适用于需要精确分数
计算的数学、科学与教育场景。`Fraction` 对象始终以最简形式存储（自动除以
最大公因数），分母始终为正数。

> **注意**：fractions 模块在初版中 API 完整，但分子/分母依赖 `int64`
> 存储，受 int64 范围（±9223372036854775807）限制；大数后端集成后将支持
> 任意精度有理数。

## 常量与类型

| 名称 | 说明 |
|---|---|
| `fractions.Fraction` | 精确有理数类 |

## 函数签名速查

**构造**

| 形式 | 说明 |
|---|---|
| `fractions.Fraction()` | 0（等同于 Fraction(0, 1)） |
| `fractions.Fraction(numerator=0, denominator=1)` | 从两个整数构造，自动化简 |
| `fractions.Fraction(value)` | 从 int、float、Decimal 或字符串构造 |
| `fractions.Fraction("3/7")` | 从分数字符串构造 |
| `fractions.Fraction("1.5")` | 从小数字符串构造 |
| `fractions.Fraction.fromFloat(f)` | 从 float 精确转换（不经字符串中转） |
| `fractions.Fraction.fromDecimal(d)` | 从 Decimal 精确转换 |

**实例方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `limitDenominator` | `f.limitDenominator(maxDenominator=1000000) → Fraction` | 求分母 ≤ maxDenominator 的最佳有理逼近 |

**实例属性**

| 属性 | 类型 | 说明 |
|---|---|---|
| `f.numerator` | `int` | 分子（化简后） |
| `f.denominator` | `int` | 分母（化简后，始终 > 0） |

**运算符**

| 运算 | 返回类型 | 说明 |
|---|---|---|
| `f + g`、`f - g`、`f * g`、`f / g` | `Fraction` | 与 Fraction 或 int 运算返回 Fraction |
| `f // g`、`f % g` | `Fraction` | 整除与取余 |
| `f ** n` | `Fraction`（n 为 int） | 幂次（负幂取倒数） |
| `f + x`（x 为 float） | `float` | 与 float 混合运算返回 float |
| `==`、`<`、`<=`、`>`、`>=` | `bool` | 精确比较，无浮点误差 |
| `abs(f)` | `Fraction` | 绝对值 |
| `round(f, ndigits=0)` | `Fraction` / `int` | 舍入 |

**内置函数支持**

通过魔法方法，以下内置函数均可作用于 `Fraction`：

| 函数 | 说明 |
|---|---|
| `math.floor(f)` | 向下取整，返回 int |
| `math.ceil(f)` | 向上取整，返回 int |
| `math.trunc(f)` | 向零截断，返回 int |
| `float(f)` | 转换为 float64（可能损失精度） |
| `int(f)` | 等同于 `math.trunc(f)` |

## 详细语义

### 构造

```ms
fractions.Fraction(3, 7)        // 3/7
fractions.Fraction(6, 8)        // 3/4（自动化简）
fractions.Fraction(-4, -6)      // 2/3（符号规范化：分母始终为正）
fractions.Fraction("3/7")       // 3/7
fractions.Fraction("1.5")       // 3/2
fractions.Fraction(1.5)         // 等同于 fromFloat(1.5) = 3/2
```

分母为 0 时抛 `ValueError`。字符串格式不合法时抛 `ValueError`。

### fromFloat 与 fromDecimal

```ms
fractions.Fraction.fromFloat(0.1)
// 3602879701896397/36028797018963968（float 0.1 的精确有理表示）

fractions.Fraction.fromDecimal(decimal.Decimal("0.1"))
// 1/10（decimal "0.1" 的精确有理表示）
```

两者的区别体现了 float 二进制表示与 decimal 十进制表示的根本差异。若需
"0.1 恰好是十分之一"，应使用 `fromDecimal` 或 `Fraction("1/10")`。

### limitDenominator

将浮点数近似为分母受限的有理数，常用于以下场景：

1. 识别 float 近似值背后的真实分数。
2. 将测量值化为可读分数。

```ms
fractions.Fraction(3.141592653589793).limitDenominator(1000)  // 355/113
fractions.Fraction(3.141592653589793).limitDenominator(100)   // 311/99
fractions.Fraction(1.0/3.0).limitDenominator()                // 1/3
```

`maxDenominator` 必须为正整数。

### 算术与混合运算

Fraction 与 Fraction 或 int 运算结果始终为 Fraction（精确）；与 float
运算时 Fraction 被转为 float，结果为 float（可能损失精度）：

```ms
fractions.Fraction(1, 3) + fractions.Fraction(1, 6)  // 1/2（精确）
fractions.Fraction(1, 3) + 1                          // 4/3（精确）
fractions.Fraction(1, 3) + 0.5                        // 0.8333...（float）
```

### 比较

比较操作不依赖浮点，完全精确：

```ms
fractions.Fraction(1, 3) == fractions.Fraction(2, 6)  // true
fractions.Fraction(1, 3) < fractions.Fraction(1, 2)   // true
fractions.Fraction(1, 3) == 1.0/3.0                   // false（float 有误差）
```

## 示例

```ms
import fractions
import math

// 基本运算
a := fractions.Fraction(1, 3)
b := fractions.Fraction(1, 6)
fmt.println(a + b)                  // 1/2
fmt.println(a * b)                  // 1/18
fmt.println(a / b)                  // 2/1（即 2）
fmt.println(a - b)                  // 1/6

// 与整数混合
fmt.println(a + 1)                  // 4/3
fmt.println(2 * a)                  // 2/3

// 属性访问
f := fractions.Fraction(22, 7)
fmt.println(f.numerator)            // 22
fmt.println(f.denominator)          // 7

// 取整
fmt.println(math.floor(fractions.Fraction(7, 2)))   // 3
fmt.println(math.ceil(fractions.Fraction(7, 2)))    // 4
fmt.println(math.trunc(fractions.Fraction(-7, 2)))  // -3

// float → 有理逼近
piApprox := fractions.Fraction(math.pi).limitDenominator(100)
fmt.println(piApprox)              // 311/99

// fromFloat vs fromDecimal
fmt.println(fractions.Fraction.fromFloat(0.1))
// 3602879701896397/36028797018963968（float 精确值）

// 字符串构造
fmt.println(fractions.Fraction("3/7") + fractions.Fraction("2/7")) // 5/7

// 精确判等（避免浮点误差）
x := fractions.Fraction(1, 10)
y := fractions.Fraction(2, 10)
z := fractions.Fraction(3, 10)
fmt.println(x + y == z)   // true（而 0.1 + 0.2 == 0.3 为 false）
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | 分母为零；字符串格式不合法；`limitDenominator` 参数 ≤ 0 |
| `TypeError` | 构造参数类型不兼容（如传入 list） |
| `OverflowError` | 初版（int64 后端）：运算结果超出 int64 范围 |
