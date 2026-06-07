# P12-T139 stdlib: math（三角 / 双曲 / 特殊函数）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `math` 模块的三角函数、双曲函数和特殊函数，补全 T138 的 math 模块。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T138 | math 基础 |

---

## API 清单

```ms
// 三角函数（弧度制）
math.sin(x)    math.cos(x)    math.tan(x)
math.asin(x)   math.acos(x)   math.atan(x)
math.atan2(y,x)         // 四象限反正切
math.degrees(x)         // 弧度 → 度
math.radians(x)         // 度 → 弧度

// 双曲函数
math.sinh(x)   math.cosh(x)   math.tanh(x)
math.asinh(x)  math.acosh(x)  math.atanh(x)

// 特殊函数
math.erf(x)            // 误差函数
math.erfc(x)           // 互补误差函数
math.gamma(x)          // Γ(x)（阶乘推广）
math.lgamma(x)         // log(|Γ(x)|)
math.tgamma(x)         // 同 gamma（C99 名称）

// 整数/位操作（Python 3.11+）
math.bit_length(n)     // 最高位位置（等价 n.bit_length()）
math.bit_count(n)      // 1 位计数（popcount）

// 统计（math 层）
math.sumprod(p, q)     // dot product（sum(p*q)），精确版
```

---

## 实现要点

```c
// 所有三角/双曲函数直接包装 <math.h>（sin/cos/tan 等）
// 域检查：asin/acos 要求 -1 <= x <= 1，否则 ValueError
// atan2(0, 0) = 0（与 C 标准一致）
// gamma(0) 和负整数 → ValueError（极点）

static MsValue mathSin(MsThread* t, MsValue* args, int argc) {
  double x; if (!msToFloat(args[0], &x)) return msRaiseTypeError(t, "sin() needs numeric");
  return MS_FLOAT_VAL(sin(x));
}
// ... 类似实现所有三角函数

// erf/erfc/gamma/lgamma 直接用 <math.h>（C99 已有）
```

---

## 验收标准（checklist）

- [ ] `math.sin(math.pi/2)` ≈ `1.0`。
- [ ] `math.atan2(1, 1)` ≈ `math.pi/4`。
- [ ] `math.gamma(5)` = `24.0`（= 4!）。
- [ ] `math.erf(0)` = `0.0`；`math.erf(inf)` = `1.0`。
- [ ] `math.degrees(math.pi)` = `180.0`。
- [ ] `math.asin(2)` → `ValueError`（域外）。

---

## 测试用例（.ms）

```ms
import math

print(math.sin(math.pi/6))   // 0.5
print(math.cos(0))            // 1.0
print(math.atan2(1, 1))      // 0.7853... (π/4)
print(math.degrees(math.pi)) // 180.0
print(math.gamma(6))         // 120.0 (= 5!)
print(math.erf(1))           // 0.8427007929...

try { math.asin(2) } catch ValueError as e { print(e.message) }
// math domain error
```

---

## Benchmark

N/A（三角函数性能由 libm 决定）。

---

## 风险与边界

- **`math.gamma` 大输入**：`gamma(171)` 溢出 double（> 1.8e308）→ `inf`（与 Python 一致，不抛异常）；`gamma(0)` / 负整数 → `ValueError`（极点）。
