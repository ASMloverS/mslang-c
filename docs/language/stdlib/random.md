# random — 伪随机数

```ms
import random
```

## 概述

基于 Mersenne Twister（MT19937）算法的伪随机数生成器，语义对齐 Python
`random` 模块。模块维护一个全局 PRNG 状态，适合单线程使用。如需并发隔离，
请使用 `random.Random` 类创建独立实例。

生成的随机数**不具备密码学安全性**，安全场景请使用 `secrets` 模块。

## 常量与类型

| 名称 | 说明 |
|---|---|
| `random.Random` | 独立 PRNG 实例类 |

## 函数签名速查

**种子与状态**

| 函数 | 签名 | 说明 |
|---|---|---|
| `seed` | `seed(a=nil)` | 初始化 PRNG；a=nil 使用系统时间 |

**整数函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `randint` | `randint(a, b) → int` | 均匀分布整数，范围 [a, b]（含两端） |
| `randrange` | `randrange(start, stop=nil, step=1) → int` | 从 range(start,stop,step) 中随机取一个整数 |

**浮点函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `random` | `random() → float` | 均匀分布 [0.0, 1.0) |
| `uniform` | `uniform(a, b) → float` | 均匀分布 [a, b] |
| `triangular` | `triangular(low=0.0, high=1.0, mode=nil) → float` | 三角分布 |
| `gauss` | `gauss(mu=0.0, sigma=1.0) → float` | 正态分布（非线程安全） |
| `normalvariate` | `normalvariate(mu=0.0, sigma=1.0) → float` | 正态分布（线程安全） |
| `lognormvariate` | `lognormvariate(mu, sigma) → float` | 对数正态分布 |
| `expovariate` | `expovariate(lambd) → float` | 指数分布，lambd = 1/均值 |
| `betavariate` | `betavariate(alpha, beta) → float` | Beta 分布，返回 [0, 1] |
| `gammavariate` | `gammavariate(alpha, beta) → float` | Gamma 分布 |
| `vonmisesvariate` | `vonmisesvariate(mu, kappa) → float` | 冯·米塞斯分布（循环数据） |
| `weibullvariate` | `weibullvariate(alpha, beta) → float` | 威布尔分布 |

**序列函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `choice` | `choice(seq) → item` | 从非空序列中随机取一个元素 |
| `choices` | `choices(seq, weights=nil, cum_weights=nil, k=1) → list` | 有放回抽样（支持加权） |
| `shuffle` | `shuffle(lst)` | 原地打乱 list（Fisher-Yates 算法） |
| `sample` | `sample(seq, k) → list` | 无放回抽样，返回 k 个唯一元素的新 list |

## 详细语义

### random.seed(a=nil)

```ms
random.seed(42)     // 固定种子，结果可复现
random.seed()       // 重新随机化（使用系统时间）
```

传入相同种子，后续调用序列完全一致。

### random.randrange(start, stop=nil, step=1)

行为与 `range(start, stop, step)` 对齐，但从中随机取值而非全部返回：

```ms
random.randrange(10)         // 等同于 randrange(0, 10, 1)，返回 [0, 9]
random.randrange(2, 20, 2)   // 从 [2, 4, 6, ..., 18] 中随机取一个偶数
```

`step` 不能为 0；范围为空时抛 `ValueError`。

### random.triangular(low, high, mode)

三角分布的众数（峰值）为 `mode`。`mode=nil` 时默认为 `(low+high)/2`（对称三角）。

### random.gauss 与 random.normalvariate

两者均生成正态分布随机数，`gauss` 速度更快但使用了内部缓存状态，不适合多
goroutine 并发调用；`normalvariate` 无缓存，线程安全。

### random.choices(seq, weights, cum_weights, k)

- `weights` 与 `cum_weights` 不可同时指定（抛 `TypeError`）。
- `weights` 为相对权重（如 `[1, 2, 1]`）；`cum_weights` 为累积权重（如
  `[1, 3, 4]`）——提前计算好可避免每次重复累加。
- k=1 时返回包含一个元素的 list（而非元素本身）。

### random.sample(seq, k)

```ms
random.sample([1, 2, 3, 4, 5], 3)   // 例如 [3, 1, 5]，无重复
```

k > len(seq) 时抛 `ValueError`。seq 本身不被修改，始终返回新 list。

### random.Random(seed=nil)

创建独立 PRNG 实例，与全局状态完全隔离：

```ms
rng1 := random.Random(123)
rng2 := random.Random(456)
rng1.random()   // 不影响 rng2 或全局状态
```

实例方法与模块级函数完全一致（`seed`、`random`、`randint`、`choice` 等）。

## 示例

```ms
import random

// 固定种子以便复现
random.seed(0)

// 基础整数与浮点
fmt.println(random.randint(1, 6))          // 模拟骰子 [1, 6]
fmt.println(random.random())               // [0.0, 1.0)
fmt.println(random.uniform(1.5, 3.5))      // [1.5, 3.5]

// 序列操作
colors := ["red", "green", "blue"]
fmt.println(random.choice(colors))

deck := []
for i := 1; i <= 52; i++ {
    deck = append(deck, i)
}
random.shuffle(deck)
hand := random.sample(deck, 5)
fmt.println(hand)

// 加权抽样（red 出现概率是 blue 的 3 倍）
fmt.println(random.choices(colors, weights=[3, 1, 1], k=10))

// 独立实例
rng := random.Random(99)
fmt.println(rng.gauss(0.0, 1.0))
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `IndexError` | `choice` 传入空序列 |
| `ValueError` | `sample` 的 k > len(seq)；`randrange` 范围为空；`betavariate`/`gammavariate` 参数 ≤ 0 |
| `TypeError` | `choices` 同时指定 `weights` 和 `cum_weights` |
