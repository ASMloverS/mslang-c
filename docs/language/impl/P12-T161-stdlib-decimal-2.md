# P12-T161 stdlib: decimal（Context / 格式化 / 高级）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

完成 `decimal` 模块的 Context 控制（精度、舍入模式、陷阱）、格式化输出、高级数学函数（sqrt/log/exp）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T160 | decimal 基础算术 |

---

## API 清单

```ms
// Context（精度与舍入控制）
ctx := decimal.getcontext()         // 获取当前线程 Context
decimal.setcontext(ctx)
decimal.localcontext(ctx=nil)       // 上下文管理器（with 语句）

ctx.prec = 50                       // 精度（有效数字位数，默认 28）
ctx.rounding = decimal.ROUND_HALF_UP  // 舍入模式
// ROUND_CEILING, ROUND_DOWN, ROUND_FLOOR, ROUND_HALF_DOWN
// ROUND_HALF_EVEN（banker's）, ROUND_HALF_UP, ROUND_UP, ROUND_05UP

ctx.traps[decimal.InvalidOperation] = false  // 禁用陷阱
ctx.flags[decimal.Inexact]          // 上次操作是否不精确

// 舍入方法
d.quantize(exp, rounding=nil) → Decimal  // 调整指数（舍入到指定位数）
// decimal.Decimal("3.14159").quantize(decimal.Decimal("0.01"))
// → Decimal("3.14")

d.round(ndigits=0) → Decimal
d.normalize() → Decimal   // 去除尾随零

// 高级数学（使用牛顿迭代/AGM 等精确算法）
d.sqrt() → Decimal
d.ln() → Decimal     // 自然对数
d.log10() → Decimal  // 以 10 为底对数
d.exp() → Decimal    // e^d

decimal.Decimal("2").sqrt()    // √2 精确到 ctx.prec 位
decimal.Decimal(1).exp()       // e，精确到 ctx.prec 位

// 格式化
format(d, ".4f")    // "3.1416"（标准 format spec）
format(d, "e")      // "3.14E+0"
format(d, "g")      // 自动选择
f"{d:.2f}"          // f-string 支持

// 常量（精确到当前精度）
decimal.pi()   // π，精确到 ctx.prec 位
decimal.e()    // e，精确到 ctx.prec 位
```

---

## 实现要点

```c
// Context 为线程局部（MsThread.decimalCtx）
// localcontext：使用 with 语句，进入时复制 ctx，退出时恢复

// quantize 实现：
// 1. 调整 coef 使 exp 匹配目标 exp（× 或 ÷ 10^n）
// 2. 若缩小精度，按 ctx.rounding 舍入

// sqrt：牛顿迭代（Newton-Raphson）
// x_{n+1} = (x_n + d/x_n) / 2
// 初始猜测：float(d).sqrt（截断到 ctx.prec 位）
// 迭代直到收敛（最后两次结果相同）

// exp：Taylor 级数（精确收敛）或 AGM 算法
// ln：使用 x = m * 2^k 分解，ln(x) = ln(m) + k*ln(2)
// 然后 ln(m)（m≈1）用级数

// pi：Machin 公式：π/4 = 4*arctan(1/5) - arctan(1/239)
// arctan 用 Gregory-Leibniz 级数加速版

// 格式化：实现 __format__ 协议
// 解析格式规格：align fill sign # 0 width , .precision type
// type: f=fixed e=scientific g=auto %=百分比 n=本地化
```

---

## 验收标准（checklist）

- [ ] 默认精度 28 位，可通过 `ctx.prec = 50` 修改。
- [ ] `Decimal("1") / Decimal("3")` 精确到 ctx.prec 位。
- [ ] `Decimal("2").sqrt()` 精确（与已知 √2 前 28 位一致）。
- [ ] `quantize(Decimal("0.01"), ROUND_HALF_UP)` 正确舍入 2.345 → 2.35。
- [ ] `localcontext` 不影响外部精度。
- [ ] `format(Decimal("3.14159"), ".3f")` → `"3.142"`。

---

## 测试用例（.ms）

```ms
import decimal

// 高精度计算
decimal.getcontext().prec = 50
two = decimal.Decimal(2)
sqrt2 = two.sqrt()
print(sqrt2)
// 1.4142135623730950488016887242096980785696718753769

// quantize 舍入
d := decimal.Decimal("3.14159")
print(d.quantize(decimal.Decimal("0.001")))  // 3.142（ROUND_HALF_EVEN）

// localcontext
with decimal.localcontext() as ctx:
    ctx.prec = 5
    print(decimal.Decimal(1) / decimal.Decimal(3))  // 0.33333（5位）
print(decimal.Decimal(1) / decimal.Decimal(3))      // 恢复 28 位

// 金融计算（ROUND_HALF_UP）
decimal.getcontext().rounding = decimal.ROUND_HALF_UP
tax_rate = decimal.Decimal("0.065")
price = decimal.Decimal("9.99")
tax = (price * tax_rate).quantize(decimal.Decimal("0.01"))
print(tax)   // 0.65

// pi 精确到 100 位
decimal.getcontext().prec = 100
print(decimal.pi())
```
