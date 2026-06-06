# P12-T159 stdlib: statistics

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `statistics` 模块（对齐 `stdlib/statistics.md`）：描述性统计函数，对整数和浮点数均适用，关键处使用精确整数算法（Fraction 辅助）避免浮点误差。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T162 | fractions（median/mean 精确计算依赖） |
| P12-T148 | sort.sort（排序用于 median/mode） |

---

## API 清单

```ms
// 集中趋势
statistics.mean(data) → float|Fraction   // 算术均值
statistics.fmean(data) → float           // 快速浮点均值（无精确保证）
statistics.geometric_mean(data) → float  // 几何均值
statistics.harmonic_mean(data, weights=nil) → float  // 调和均值
statistics.median(data) → float|Fraction // 中位数
statistics.median_low(data) → value      // 低中位数（偶数长取小的）
statistics.median_high(data) → value     // 高中位数（偶数长取大的）
statistics.median_grouped(data, interval=1) → float  // 分组中位数
statistics.mode(data) → value            // 众数（多个时取最早出现的）
statistics.multimode(data) → list        // 所有众数
statistics.quantiles(data, n=4, method="exclusive") → list  // n 分位数

// 离散程度
statistics.pstdev(data, mu=nil) → float     // 总体标准差
statistics.pvariance(data, mu=nil) → float  // 总体方差
statistics.stdev(data, xbar=nil) → float    // 样本标准差（Bessel 校正）
statistics.variance(data, xbar=nil) → float // 样本方差

// 相关
statistics.covariance(x, y) → float
statistics.correlation(x, y, method="linear") → float
statistics.linear_regression(x, y) → LinearRegression

// 分布
statistics.NormalDist(mu=0.0, sigma=1.0)
// .pdf(x)  .cdf(x)  .quantile(p)  .overlap(other) ...

// 异常
statistics.StatisticsError
```

---

## 实现要点

```c
// mean：使用 Fraction 精确累加（避免浮点误差）
// mean([0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1]) = 0.1 (精确)

// stdev/variance：两遍法（先算 mean，再累加 (x-mean)^2）
// 数值稳定：Welford 在线算法（一遍法）

// median：排序后取中点（奇数 = 中间，偶数 = 两中间的 Fraction 均值）
// median_grouped：CPython 实现算法（基于插值）

// mode：线性扫描计数，取最高频
// multimode：同 mode 但返回所有并列最高频

// quantiles：分位数计算（exclusive/inclusive 两种方法）

// linear_regression：普通最小二乘（OLS）
// 斜率 = Cov(x,y) / Var(x)，截距 = ȳ - slope * x̄

// NormalDist：
// pdf(x) = exp(-(x-mu)^2/(2*sigma^2)) / (sigma*sqrt(2*pi))
// cdf：使用误差函数近似 erf（调用 math.erf）
```

---

## 验收标准（checklist）

- [ ] `statistics.mean([1,2,3,4,5])` → `3.0`（精确）。
- [ ] `statistics.mean([0.1]*10)` → `0.1`（精确，无浮点误差）。
- [ ] `statistics.median([1,3,5,7])` → `4.0`（两中间均值）。
- [ ] `statistics.mode([1,1,2,3])` → `1`。
- [ ] `statistics.stdev([2,4,4,4,5,5,7,9])` → `2.0`。
- [ ] `statistics.correlation([1,2,3],[1,2,3])` → `1.0`。

---

## 测试用例（.ms）

```ms
import statistics as st

data := [2, 4, 4, 4, 5, 5, 7, 9]
print(st.mean(data))      // 5.0
print(st.median(data))    // 4.5
print(st.mode(data))      // 4
print(st.variance(data))  // 4.571...
print(st.stdev(data))     // 2.138...

// 精确均值（Fraction）
print(st.mean([1, 3]))    // Fraction(2, 1) 或 2.0

// 线性回归
x := [1, 2, 3, 4, 5]
y := [2, 4, 5, 4, 5]
reg := st.linear_regression(x, y)
print(reg.slope)      // ≈ 0.6
print(reg.intercept)  // ≈ 2.2

// 正态分布
nd := st.NormalDist(mu=100, sigma=15)
print(nd.cdf(115))   // ≈ 0.8413（均值+1σ 以下概率）
print(nd.pdf(100))   // 峰值概率密度
```
