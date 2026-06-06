# P12-T160 stdlib: decimal（基础算术）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `decimal` 模块的任意精度十进制算术（对齐 `stdlib/decimal.md`）。核心：基于 BCD（Binary-Coded Decimal）或大整数表示，支持精确十进制小数，零外部依赖。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | MsStrObj（字符串解析输入） |
| P4-T053 | int 算术（大整数基础） |

---

## API 清单（本任务范围：基础算术）

```ms
// 构造
decimal.Decimal("3.14")      // 从字符串（推荐，精确）
decimal.Decimal(3)           // 从 int
decimal.Decimal("Inf")       // 正无穷
decimal.Decimal("NaN")       // 非数字
decimal.Decimal("sNaN")      // signaling NaN

d := decimal.Decimal("1.05")

// 基础算术（本任务）
d + d2     // 加
d - d2     // 减
d * d2     // 乘
d / d2     // 除（按当前 Context 精度）
d // d2    // 整除
d % d2     // 取余
d ** n     // 幂（整数指数）
-d         // 取负
+d         // 取正（返回副本）
abs(d)

// 比较（精确）
d == d2    // 0.1 + 0.2 != 0.3（不同于 float）
d < d2     // 数值比较（NaN 比较抛 InvalidOperation）

// 属性
d.sign     // 0=正 1=负
d.coefficient   // int（无精度信息的整数值）
d.exponent      // int（10 的幂次）
// 实际值 = coefficient * 10^exponent

// 转换
int(d)     // 截断为 int（保留整数部分）
float(d)   // 转 float（可能有精度损失）
str(d)     // → "3.14"
repr(d)    // → "Decimal('3.14')"
```

---

## 实现要点

```c
// Decimal 内部表示：(sign, coef, exp)
// sign: 0=正 1=负
// coef: MsBigInt（任意精度非负整数，作为 limb 数组）
// exp: int32_t（指数，负数=小数点后几位）
// 值 = (-1)^sign × coef × 10^exp

// 加法：对齐指数（较小指数的系数 × 10^|exp_diff|），然后加
// 乘法：coef 相乘（大整数乘），指数相加
// 除法：扩展被除数精度（× 10^precision），整除，舍入

// 大整数（limb 数组）实现：
// 使用 uint32_t limb，基数 10^9（十亿）
// 乘法：O(n^2) schoolbook；后续可升级 Karatsuba

typedef struct MsDecimalObj {
    MsObject header;
    int8_t   sign;      // 0 or 1
    uint8_t  isSpecial; // 0=normal 1=Inf 2=NaN 3=sNaN
    uint32_t* limbs;    // 系数（little-endian base 10^9）
    uint32_t  nlimbs;
    int32_t  exp;       // 指数
} MsDecimalObj;

// 规范化：去除前导零 limb，调整 exp（trailing zeros 可选保留或去除，
// 由 Context.prec 决定舍入）
```

---

## 验收标准（checklist）

- [ ] `Decimal("0.1") + Decimal("0.2") == Decimal("0.3")` → `true`（精确）。
- [ ] `Decimal("1") / Decimal("3")` 精确到当前精度（默认 28 位）。
- [ ] `Decimal("1.5") * Decimal("2.5")` → `Decimal("3.75")`（精确）。
- [ ] `Decimal("Inf") + Decimal("1")` → `Decimal("Inf")`。
- [ ] `Decimal("NaN") + Decimal("1")` → `Decimal("NaN")`。
- [ ] `str(Decimal("3.140"))` → `"3.140"`（保留尾随零）。

---

## 测试用例（.ms）

```ms
import decimal

// 精确十进制
print(decimal.Decimal("0.1") + decimal.Decimal("0.2"))
// 0.3  （不像 float 的 0.30000000000000004）

// 精确乘法
a := decimal.Decimal("1.1") ** 10
print(a)  // 2.59374246010...（精确 28 位）

// 整数除法
print(decimal.Decimal("10") // decimal.Decimal("3"))  // 3
print(decimal.Decimal("10") %  decimal.Decimal("3"))  // 1

// 特殊值
inf := decimal.Decimal("Inf")
print(inf + 1)    // Infinity
print(inf - inf)  // NaN

// 比较
print(decimal.Decimal("1.0") == decimal.Decimal("1"))  // true（数值相等）
print(decimal.Decimal("1.0").compare(decimal.Decimal("1")))  // 0
```
