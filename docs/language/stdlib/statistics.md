# statistics — 基础统计函数

```ms
import statistics
```

## 概述

基于 `float64` 的统计描述函数，参考 Python `statistics` 模块设计，适用于
小至中型数据集的统计分析。所有函数接受任意可迭代对象（list、tuple、
generator 等）作为数据参数。大规模数据分析建议使用专用数据分析库。

数值参数支持 `int` 与 `float`；传入 `Decimal` 或 `Fraction` 时行为视实现
而定，初版不保证支持。

## 常量与类型

| 名称 | 说明 |
|---|---|
| `statistics.StatisticsError` | 本模块专用异常，`ValueError` 的子类 |

## 函数签名速查

**集中趋势**

| 函数 | 签名 | 说明 |
|---|---|---|
| `mean` | `mean(data) → float` | 算术平均值 |
| `fmean` | `fmean(data) → float` | 快速浮点均值 |
| `geometricMean` | `geometricMean(data) → float` | 几何平均值 |
| `harmonicMean` | `harmonicMean(data, weights=nil) → float` | 调和平均值 |
| `median` | `median(data) → float` | 中位数（偶数个时取均值插值） |
| `medianLow` | `medianLow(data)` | 取下中位数 |
| `medianHigh` | `medianHigh(data)` | 取上中位数 |
| `medianGrouped` | `medianGrouped(data, interval=1) → float` | 分组连续数据中位数 |
| `mode` | `mode(data)` | 众数（唯一；多众数时抛异常） |
| `multimode` | `multimode(data) → list` | 所有众数列表 |
| `quantiles` | `quantiles(data, n=4, method="exclusive") → list` | 等分位数 |

**离散程度**

| 函数 | 签名 | 说明 |
|---|---|---|
| `pstdev` | `pstdev(data, mu=nil) → float` | 总体标准差 |
| `pvariance` | `pvariance(data, mu=nil) → float` | 总体方差 |
| `stdev` | `stdev(data, xbar=nil) → float` | 样本标准差（贝塞尔校正） |
| `variance` | `variance(data, xbar=nil) → float` | 样本方差（贝塞尔校正） |

**相关性**

| 函数 | 签名 | 说明 |
|---|---|---|
| `covariance` | `covariance(x, y) → float` | 样本协方差 |
| `correlation` | `correlation(x, y) → float` | 皮尔逊相关系数 [−1, 1] |
| `linearRegression` | `linearRegression(x, y) → (float, float)` | 简单线性回归，返回 (slope, intercept) |

## 详细语义

### 集中趋势

#### mean / fmean

`mean` 使用精确有理数算法计算均值，保证整数数据的精确结果；`fmean` 将数
据直接转为 float 累加，速度更快但精度略低。两函数在数据为空时均抛
`StatisticsError`。

#### geometricMean

几何平均值 = (x₁ × x₂ × … × xₙ)^(1/n)。所有元素必须严格为正数；包含
零或负数时抛 `ValueError`。

#### harmonicMean

调和平均值 = n / (1/x₁ + 1/x₂ + … + 1/xₙ)。`weights` 为可选权重序列，
长度须与 `data` 相同。数据含零或负数时抛 `StatisticsError`。

#### median / medianLow / medianHigh

```ms
statistics.median([1, 3, 5])          // 3.0
statistics.median([1, 3, 5, 7])       // 4.0（(3+5)/2 插值）
statistics.medianLow([1, 3, 5, 7])   // 3
statistics.medianHigh([1, 3, 5, 7])  // 5
```

`medianLow` 与 `medianHigh` 始终返回序列中的实际值（不插值），因此返
回类型与输入元素类型相同。

#### medianGrouped

将数据视为来自分组连续分布的样本，使用区间插值计算中位数：

```ms
statistics.medianGrouped([52, 52, 53, 54], interval=1)  // 52.5
```

#### mode / multimode

```ms
statistics.mode([1, 2, 2, 3])        // 2
statistics.mode([1, 2])              // StatisticsError：无唯一众数
statistics.multimode([1, 2, 2, 3, 3]) // [2, 3]
statistics.multimode([1, 2, 3])       // [1, 2, 3]（全部等频）
```

`multimode` 按首次出现顺序排列，永不抛错。

#### quantiles(data, n=4, method)

将数据分为 n 个等概率区间，返回 n−1 个分位点的 list。`n=4` 返回四分位
数 [Q1, Q2, Q3]。

`method` 取值：
- `"exclusive"`（默认）：分位点不包含端点，适合连续分布。
- `"inclusive"`：分位点包含端点，适合小样本离散数据。

数据少于 2 个时抛 `StatisticsError`。

### 离散程度

#### variance / stdev（样本）

使用 n−1 作为分母（贝塞尔校正），适用于从总体中抽取的样本。数据少于 2
个时抛 `StatisticsError`。

`xbar` 为预先计算好的均值；省略时函数自动计算，传入可节省重复计算开销。

#### pvariance / pstdev（总体）

使用 n 作为分母，适用于已知整个总体的情况。`mu` 为预先计算好的总体均值。
数据为空时抛 `StatisticsError`。

### 相关性

#### covariance(x, y)

样本协方差，要求 x 与 y 长度相同且均至少 2 个元素，否则抛
`StatisticsError`。

#### correlation(x, y)

皮尔逊相关系数，返回 [−1, 1]。+1 表示完全正相关，−1 表示完全负相关，0
表示不相关。若 x 或 y 的方差为零（所有元素相同），抛 `StatisticsError`。

#### linearRegression(x, y)

使用最小二乘法拟合 y = slope × x + intercept。返回 (slope, intercept)
tuple：

```ms
slope, intercept := statistics.linearRegression(
    [1.0, 2.0, 3.0],
    [2.0, 4.0, 5.0],
)
// slope ≈ 1.5, intercept ≈ 0.33...
```

## 示例

```ms
import statistics

data := [2, 4, 4, 4, 5, 5, 7, 9]

fmt.println(statistics.mean(data))       // 5.0
fmt.println(statistics.median(data))     // 4.5
fmt.println(statistics.mode(data))       // 4
fmt.println(statistics.stdev(data))      // 2.0
fmt.println(statistics.variance(data))   // 4.0
fmt.println(statistics.pstdev(data))     // 1.8708...

// 四分位数
fmt.println(statistics.quantiles(data))  // [4.0, 4.5, 5.5]

// 相关性分析
x := [1.0, 2.0, 3.0, 4.0, 5.0]
y := [2.1, 3.9, 6.2, 8.0, 9.8]
fmt.println(statistics.correlation(x, y))          // ≈ 0.9998
slope, intercept := statistics.linearRegression(x, y)
fmt.printf("y = %.2fx + %.2f\n", slope, intercept) // y = 1.96x + 0.14

// 众数
words := ["apple", "banana", "apple", "cherry", "banana", "apple"]
fmt.println(statistics.mode(words))      // "apple"
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `statistics.StatisticsError` | 数据为空；样本方差/协方差数据不足 2 个；`mode` 无唯一众数；`correlation` 方差为零等 |
| `ValueError` | `geometricMean` 含零或负数；`harmonicMean` 含零或负数 |
| `TypeError` | 数据元素类型不兼容（如混入字符串） |
