# decimal — 任意精度十进制浮点

```ms
import decimal
```

## 概述

精确的十进制浮点运算，避免 `float64` 二进制表示带来的精度问题。适用于金
融计算、货币处理等需要精确十进制表示的场景（例如 `0.1 + 0.2` 在 decimal
中恰好等于 `0.3`）。

语义参考 IBM 通用十进制算术规范（General Decimal Arithmetic）与 Python
`decimal` 模块。

> **注意**：decimal 模块在初版中 API 完整，但依赖后续大数运算后端支持；
> 在大数后端集成前，有效精度上限受底层实现影响，超出部分可能触发
> `Inexact` 信号。

## 常量与类型

| 名称 | 说明 |
|---|---|
| `decimal.Decimal` | 任意精度十进制浮点数类 |
| `decimal.Context` | 运算上下文（精度、舍入模式、信号配置） |
| `decimal.ROUND_HALF_EVEN` | 舍入到最近偶数（银行家舍入，默认） |
| `decimal.ROUND_HALF_UP` | 四舍五入（远离零） |
| `decimal.ROUND_HALF_DOWN` | 五舍六入（趋向零） |
| `decimal.ROUND_UP` | 远离零方向进位 |
| `decimal.ROUND_DOWN` | 趋向零方向截断 |
| `decimal.ROUND_CEILING` | 向正无穷方向取整 |
| `decimal.ROUND_FLOOR` | 向负无穷方向取整 |
| `decimal.ROUND_05UP` | 若截断后末位为 0 或 5 则进位，否则截断 |
| `decimal.InvalidOperation` | 无效运算信号/异常 |
| `decimal.DivisionByZero` | 除以零信号/异常 |
| `decimal.Overflow` | 溢出信号/异常 |
| `decimal.Underflow` | 下溢信号/异常 |
| `decimal.Inexact` | 结果不精确信号/异常 |

## 函数签名速查

**Decimal 构造**

| 形式 | 说明 |
|---|---|
| `decimal.Decimal(value)` | value 为 int、str 或 (sign, digits_tuple, exponent) tuple |
| `decimal.Decimal("3.14")` | 从字符串构造（推荐方式） |
| `decimal.Decimal("Infinity")` | 正无穷 |
| `decimal.Decimal("NaN")` | 安静 NaN |
| `decimal.Decimal("sNaN")` | 信号 NaN（运算时触发 InvalidOperation） |

**Decimal 实例方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `sqrt` | `d.sqrt() → Decimal` | 平方根 |
| `ln` | `d.ln() → Decimal` | 自然对数 |
| `log10` | `d.log10() → Decimal` | 以 10 为底的对数 |
| `exp` | `d.exp() → Decimal` | e^d |
| `quantize` | `d.quantize(exp, rounding=nil) → Decimal` | 按给定指数舍入 |
| `normalize` | `d.normalize() → Decimal` | 去除尾随零 |
| `toIntegralValue` | `d.toIntegralValue(rounding=nil) → Decimal` | 舍入到整数值 |
| `asIntegerRatio` | `d.asIntegerRatio() → (int, int)` | 返回等值分数 (numerator, denominator) |
| `isNan` | `d.isNan() → bool` | 是否为 NaN（含 sNaN） |
| `isInfinite` | `d.isInfinite() → bool` | 是否为 ±∞ |
| `isFinite` | `d.isFinite() → bool` | 是否为有限数 |

**Decimal 属性**

| 属性 | 说明 |
|---|---|
| `d.sign` | 0 表示正数或正零，1 表示负数或负零 |

**上下文函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `getcontext` | `decimal.getcontext() → Context` | 获取当前线程上下文 |
| `setcontext` | `decimal.setcontext(ctx)` | 设置当前线程上下文 |
| `localcontext` | `decimal.localcontext(ctx=nil) → ContextManager` | 临时上下文（配合 `with` 使用） |
| `Context` | `decimal.Context(prec=28, rounding="ROUND_HALF_EVEN", ...)` | 创建新上下文 |

## 详细语义

### Decimal 构造

```ms
// 推荐：从字符串构造，精确
decimal.Decimal("0.1")
decimal.Decimal("3.14159265358979323846")

// 从 int 构造，精确
decimal.Decimal(42)

// 从 float 构造——不推荐，会继承 float 的二进制近似误差
decimal.Decimal(0.1)          // 0.1000000000000000055511151231257827021181583404541015625
decimal.Decimal(str(0.1))     // "0.1"，使用字符串中转

// 从 tuple 构造：(sign, digits, exponent)
decimal.Decimal((0, (3, 1, 4), -2))   // 3.14，sign=0 为正
decimal.Decimal((1, (3, 1, 4), -2))   // -3.14，sign=1 为负
```

### 算术运算

`Decimal` 支持 `+`、`-`、`*`、`/`、`//`、`%`、`**`，与 `int` 混合运算
返回 `Decimal`。**不支持与 `float` 直接运算**（抛 `TypeError`）；需先用
`Decimal(str(f))` 转换。

### quantize — 精确舍入

```ms
d := decimal.Decimal("1.41421356")
d.quantize(decimal.Decimal("0.01"))              // Decimal("1.41")
d.quantize(decimal.Decimal("0.001"), rounding=decimal.ROUND_UP) // Decimal("1.415")
d.quantize(decimal.Decimal("1"))                 // Decimal("1")
```

金融场景中常用于将结果舍入到指定小数位。

### normalize

```ms
decimal.Decimal("1.300").normalize()   // Decimal("1.3")
decimal.Decimal("1.00").normalize()    // Decimal("1")
decimal.Decimal("0.00").normalize()    // Decimal("0")
```

### Context 与精度控制

```ms
// 修改全局精度
ctx := decimal.getcontext()
ctx.prec = 50

// 临时高精度计算（不影响全局）
with decimal.localcontext() as ctx {
    ctx.prec = 100
    result := decimal.Decimal("2").sqrt()
}
// 离开 with 块后精度恢复
```

`Context` 的主要字段：
- `prec`：有效位数（默认 28）
- `rounding`：舍入模式（见常量表）
- `Emax`、`Emin`：指数范围

### 信号与异常配置

Context 中每种信号（`InvalidOperation`、`DivisionByZero` 等）默认配置为
是否抛出异常。可在上下文中将信号设为静默（仅记录标志）或抛出异常：

```ms
ctx := decimal.getcontext()
// 让 Inexact 仅记录标志而不抛异常
ctx.traps[decimal.Inexact] = false
```

## 示例

```ms
import decimal

// 精确货币计算
price := decimal.Decimal("19.99")
taxRate := decimal.Decimal("0.08")
tax := price * taxRate
total := price + tax
// 舍入到分
total = total.quantize(decimal.Decimal("0.01"))
fmt.println(total)   // 21.59

// 高精度 pi（需先提升 prec）
with decimal.localcontext() as ctx {
    ctx.prec = 50
    // 使用 Machin 公式或标准库函数
    piApprox := decimal.Decimal(4) * (
        decimal.Decimal(4) * decimal.Decimal("0.2").atanManual()
    )
}

// 避免 float 精度问题
a := decimal.Decimal("0.1") + decimal.Decimal("0.2")
fmt.println(a)                        // 0.3
fmt.println(a == decimal.Decimal("0.3"))  // true

// 特殊值
inf := decimal.Decimal("Infinity")
nan := decimal.Decimal("NaN")
fmt.println(nan.isNan())      // true
fmt.println(inf.isInfinite()) // true
fmt.println(inf.isFinite())   // false
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `decimal.InvalidOperation` | 无效运算，如 `0 / 0`、`Infinity − Infinity`、对 NaN 运算 |
| `decimal.DivisionByZero` | 有限数除以 Decimal("0") |
| `decimal.Overflow` | 结果指数超出 Emax |
| `decimal.Underflow` | 结果过小无法以最小精度表示 |
| `decimal.Inexact` | 结果在当前精度下无法精确表示（默认静默） |
| `TypeError` | Decimal 与 float 直接运算 |
