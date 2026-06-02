# itertools — 惰性迭代器组合工具

```ms
import itertools
```

## 概述

受 Python `itertools` 和 Haskell 列表函数启发的高效迭代器构建工具。所有函数均为惰性（返回迭代器，
实现 `__iter__`/`__next__`），不提前计算全部结果。适用于处理大型或无限序列，
与内置 `map`/`filter`/`zip` 配合使用可构造高效数据流水线。

## 常量与类型

本模块不导出常量或类型，所有成员均为函数（或函数对象）。

## 函数签名速查

**无限迭代器**

| 函数 | 签名 | 说明 |
|---|---|---|
| `count` | `count(start=0, step=1)` | 从 start 开始无限计数 |
| `cycle` | `cycle(iter)` | 无限循环遍历 iter |
| `repeat` | `repeat(obj, times=nil)` | 重复 obj；times 不为 nil 时有限 |

**有限迭代器**

| 函数 | 签名 | 说明 |
|---|---|---|
| `chain` | `chain(*iters)` | 顺序拼接多个可迭代对象 |
| `chain.from_iterable` | `chain.from_iterable(iter_of_iters)` | 惰性版链接 |
| `islice` | `islice(iter, stop)` / `islice(iter, start, stop[, step])` | 切片迭代器 |
| `takewhile` | `takewhile(pred, iter)` | 谓词为真时持续取值 |
| `dropwhile` | `dropwhile(pred, iter)` | 谓词为真时跳过，之后全部返回 |
| `filterfalse` | `filterfalse(pred, iter)` | 返回谓词为假的元素 |
| `compress` | `compress(data, selectors)` | 按选择器掩码过滤数据 |
| `starmap` | `starmap(fn, iter)` | 将每项解包后作为参数调用 fn |
| `pairwise` | `pairwise(iter)` | 相邻元素两两配对 |
| `batched` | `batched(iter, n)` | 按固定大小分批 |
| `accumulate` | `accumulate(iter, fn=operator.add, initial=nil)` | 累积运算（running totals） |
| `groupby` | `groupby(iter, key=nil)` | 对连续相同键的元素分组 |
| `zip_longest` | `zip_longest(*iters, fillvalue=nil)` | 以最长序列为基准 zip |

**组合迭代器**

| 函数 | 签名 | 说明 |
|---|---|---|
| `product` | `product(*iters, repeat=1)` | 笛卡尔积 |
| `permutations` | `permutations(iter, r=nil)` | r 长度排列 |
| `combinations` | `combinations(iter, r)` | r 长度组合（不重复） |
| `combinations_with_replacement` | `combinations_with_replacement(iter, r)` | 允许重复的组合 |

## 详细语义

### 无限迭代器

#### count

```
itertools.count(start=0, step=1)
```

从 `start` 开始，每次加 `step`，无限产生值：`start, start+step, start+2*step, ...`。
`step` 可为负数或浮点数。必须与 `islice` 或 `takewhile` 配合使用才能终止。

#### cycle

```
itertools.cycle(iter)
```

将 `iter` 中的元素缓存后无限循环输出。内部保存一份拷贝，即使原迭代器已耗尽也能重复。
若 `iter` 为空，则产生零个元素。

#### repeat

```
itertools.repeat(obj, times=nil)
```

无限（或 `times` 次）重复产生 `obj`。`times=0` 时立即耗尽。常与 `map`/`zip` 配合
向迭代器注入常量值。

---

### 有限迭代器

#### chain

```
itertools.chain(*iters)
```

顺序拼接多个可迭代对象，前一个耗尽后再从下一个取值。不复制数据。

#### chain.from_iterable

```
itertools.chain.from_iterable(iter_of_iters)
```

惰性展平一层嵌套迭代器。等价于 `chain(*iters)`，但 `iter_of_iters` 本身也是惰性求值的。

```ms
// chain(["AB", "CD"]) → 'A','B','C','D'
for ch in itertools.chain.from_iterable(["AB", "CD"]) {
    fmt.print(ch)
}
```

#### islice

```
itertools.islice(iter, stop)
itertools.islice(iter, start, stop[, step])
```

对迭代器做切片，语义与列表切片相同，但不支持负索引。`stop=nil` 表示取到末尾。
`step` 必须为正整数。跳过的元素仍会被消费（向前推进迭代器）。

#### takewhile

```
itertools.takewhile(pred, iter)
```

依次取元素，直到 `pred(x)` 为假则立即停止（含该元素不输出）。之后的元素不再消费。

#### dropwhile

```
itertools.dropwhile(pred, iter)
```

跳过满足 `pred(x)` 为真的前缀元素，一旦 `pred` 返回假则输出该元素及后续所有元素。

#### filterfalse

```
itertools.filterfalse(pred, iter)
```

返回 `pred(x)` 为假的元素，与内置 `filter` 互补。`pred=nil` 时过滤假值元素。

#### compress

```
itertools.compress(data, selectors)
```

将 `data` 和 `selectors` 并行消费，仅输出对应选择器为真的数据元素。
两者中较短的一方耗尽时停止。

#### starmap

```
itertools.starmap(fn, iter)
```

对 `iter` 中每个元素 `item` 调用 `fn(*item)`，将元素解包为位置参数。
`item` 必须为可迭代对象。

#### pairwise

```
itertools.pairwise(iter)
```

产生相邻元素对 `(a[0],a[1]), (a[1],a[2]), ...`。若输入少于 2 个元素则不产生任何对。

#### batched

```
itertools.batched(iter, n)
```

将迭代器按 `n` 个元素一组打包为 `tuple`。最后一批可能不足 `n` 个元素。
`n` 必须为正整数，否则抛 `ValueError`。

#### accumulate

```
itertools.accumulate(iter, fn=operator.add, initial=nil)
```

产生累积运算结果序列。`initial` 不为 nil 时作为首个输出值（在 `iter` 首元素之前）。
`fn` 接受 `(acc, x)` 两个参数。`iter` 为空且无 `initial` 时产生零个元素。

#### groupby

```
itertools.groupby(iter, key=nil)
```

对 `iter` 中连续的、键相同的元素分组，产生 `(key_val, group_iter)` 对。
`key=nil` 时以元素本身为键。**输入必须预先按键排序**，否则相同键的元素可能出现在多个组中。
每次调用 `__next__` 获取下一组时，前一组的 `group_iter` 会失效。

#### zip_longest

```
itertools.zip_longest(*iters, fillvalue=nil)
```

以最长迭代器为准进行 zip，较短的迭代器耗尽后以 `fillvalue` 填充。

---

### 组合迭代器

#### product

```
itertools.product(*iters, repeat=1)
```

计算多个可迭代对象的笛卡尔积，等价于嵌套 `for` 循环。
`repeat=n` 时将输入序列重复 `n` 次再求积。输出顺序为字典序（最右侧迭代器先变化）。

#### permutations

```
itertools.permutations(iter, r=nil)
```

产生 `r` 长度的排列（无重复元素）。`r=nil` 时取全排列（长度等于输入长度）。
若 `r` 大于输入长度则不产生任何元素。输出顺序按输入的字典序排列。

#### combinations

```
itertools.combinations(iter, r)
```

产生 `r` 长度的组合（不含重复，元素按输入顺序）。输出按字典序排列。

#### combinations_with_replacement

```
itertools.combinations_with_replacement(iter, r)
```

允许元素重复出现的组合，每个位置可从输入中重复选取。

## 示例

```ms
import itertools

// 1. islice + count：取前 5 个偶数
evens := itertools.islice(
    itertools.filterfalse(func(x) { return x % 2 != 0 }, itertools.count()),
    5
)
fmt.println(list(evens))  // [0, 2, 4, 6, 8]

// 2. accumulate：running sum
nums := [1, 2, 3, 4, 5]
fmt.println(list(itertools.accumulate(nums)))  // [1, 3, 6, 10, 15]

// 3. groupby：按单词长度分组（输入需预先排序）
words := ["a", "be", "go", "cat", "dog", "four"]
words = sorted(words, key=func(w) { return len(w) })
for k, g in itertools.groupby(words, key=func(w) { return len(w) }) {
    fmt.println(k, list(g))
}
// 1 ["a"]
// 2 ["be", "go"]
// 3 ["cat", "dog"]
// 4 ["four"]

// 4. product：生成 2×2 网格坐标
for pt in itertools.product([0, 1], [0, 1]) {
    fmt.println(pt)
}
// (0, 0)  (0, 1)  (1, 0)  (1, 1)

// 5. chain + islice：分页
pages := [["p1a", "p1b"], ["p2a", "p2b"], ["p3a"]]
all_items := itertools.chain.from_iterable(pages)
fmt.println(list(itertools.islice(all_items, 3)))  // ["p1a", "p1b", "p2a"]

// 6. batched：将流按块处理
for batch in itertools.batched(range(10), 3) {
    fmt.println(batch)
}
// (0,1,2)  (3,4,5)  (6,7,8)  (9,)
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `islice` 的 start/stop/step 为负数或 step 为零；`batched` 的 n ≤ 0 |
| `TypeError` | 传入不可迭代对象；`starmap` 的元素不可解包；`accumulate` 的 fn 不可调用 |
| `StopIteration` | 所有迭代器正常耗尽时内部抛出，由 `for` 循环或 `next()` 处理 |
