# P12-T158 stdlib: random

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `random` 模块（对齐 `stdlib/random.md`），核心使用 **Mersenne Twister (MT19937)** 算法，全自实现，零外部依赖。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T054 | float 算术（randint/random 返回浮点） |
| P12-T136 | os.urandom（种子来源） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-random.md` | §1 模块 API |

---

## API 清单

```ms
// 种子与状态
random.seed(a=nil)         // nil=使用 os.urandom 种子化
random.getstate() → state  // 返回 MT 状态（不透明对象）
random.setstate(state)     // 恢复状态

// 核心
random.random() → float    // [0.0, 1.0) 均匀分布
random.uniform(a, b) → float  // [a, b]
random.randint(a, b) → int    // [a, b] 闭区间整数

// 整数
random.randrange(stop)              // [0, stop)
random.randrange(start, stop, step=1)  // range 中随机取
random.getrandbits(k) → int        // k 位随机整数

// 序列操作
random.choice(seq) → elem     // 随机选一个
random.choices(seq, weights=nil, k=1) → list  // 有权重抽样（放回）
random.shuffle(lst)            // 原地随机打乱（Fisher-Yates）
random.sample(seq, k) → list  // 无放回抽样 k 个

// 分布
random.gauss(mu=0.0, sigma=1.0) → float   // 正态分布（Box-Muller）
random.normalvariate(mu, sigma) → float   // 正态（Kinderman-Monahan）
random.expovariate(lambd) → float         // 指数分布
random.betavariate(alpha, beta) → float   // Beta 分布
random.gammavariate(alpha, beta) → float  // Gamma 分布
random.triangular(low=0.0, high=1.0, mode=nil) → float

// 模块级便捷函数（使用全局 Random 实例）
// 同上，不加 random. 前缀

// Random 类（可实例化多个独立 RNG）
r := random.Random(seed=42)
r.random()  r.randint(a, b)  // 等
```

---

## 实现要点

```c
// Mersenne Twister MT19937：
// 状态：624 个 uint32_t（mt[N]）+ index
#define MT_N 624
#define MT_M 397
#define MT_MATRIX_A 0x9908b0dfUL
#define MT_UPPER_MASK 0x80000000UL
#define MT_LOWER_MASK 0x7fffffffUL

typedef struct MsRandomObj {
  MsObject header;
  uint32_t mt[MT_N];
  int      index;
} MsRandomObj;

// genrand_int32：生成下一个 32 位随机整数
// 每 N 步重新生成：twist()

// random()：genrand_int32() 转 [0,1)：
// return (genrand_int32() >> 5) * (1.0 / (1<<27));
// (使用 53 位精度：两个 32 位数组合)

// randint(a,b)：(b-a+1) 范围内均匀整数，用拒绝采样避免模偏差
// 拒绝采样：生成 [0, 2^k) 中的随机整数，丢弃 >= range 的

// gauss：Box-Muller 变换：
// u1,u2=random(); z=sqrt(-2*log(u1))*cos(2*pi*u2); return mu+sigma*z

// shuffle：Fisher-Yates:
// for i in range(n-1, 0, -1): j=randint(0,i); swap(lst[i], lst[j])

// choices with weights：前缀和 + bisect_right（O(log n) 每次）

// getstate/setstate：序列化/反序列化 mt[624] + index
```

---

## 验收标准（checklist）

- [ ] `random.seed(42); random.random()` 值固定（可重现）。
- [ ] `random.randint(1, 6)` 结果在 [1,6] 区间，分布均匀（chi-squared 通过）。
- [ ] `random.shuffle([1,2,3,4,5])` 统计分布正确（120 种等概率）。
- [ ] `random.gauss(0,1)` 均值趋近 0，方差趋近 1（10K 样本）。
- [ ] `random.getstate()/setstate()` 实现确定性复现。
- [ ] `random.choices("AB", weights=[1,2], k=1000)` B 占比约 67%。

---

## 测试用例（.ms）

```ms
import random

// 可重现
random.seed(12345)
vals := [random.random() for _ in range(5)]
random.seed(12345)
print([random.random() for _ in range(5)] == vals)  // true

// 分布测试
counts := {1:0, 2:0, 3:0, 4:0, 5:0, 6:0}
for _ in range(6000) { counts[random.randint(1,6)] += 1 }
for k, v in counts.items() {
    print(k, v, "≈1000")   // 各约 1000
}

// shuffle
lst := list(range(10))
random.shuffle(lst)
print(lst)   // 乱序

// weighted choices
from collections import Counter
c := Counter(random.choices("abc", weights=[1,2,3], k=6000))
print(c["a"], "≈1000", c["b"], "≈2000", c["c"], "≈3000")
```

---

## Benchmark

```ms
import random, time
n := 10_000_000
t0 := time.now()
for _ in range(n) { random.random() }
t1 := time.now()
print("10M random():", t1-t0, "ms")  // 目标 < 500ms
```
