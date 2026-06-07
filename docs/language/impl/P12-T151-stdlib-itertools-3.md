# P12-T151 stdlib: itertools（过滤 / 分组 / 工具）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `itertools` 的过滤、分组、切片等工具迭代器，完成 `itertools` 模块。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T149 | itertools 无限迭代器（含 islice） |
| P12-T150 | itertools 组合迭代器 |

---

## API 清单

```ms
// 过滤/选取
itertools.compress(data, selectors)          // [d for d,s in zip(data,sel) if s]
itertools.filterfalse(predicate, iterable)   // pred 为 false 时选取
itertools.dropwhile(predicate, iterable)     // 丢弃前缀 true，之后全取
itertools.takewhile(predicate, iterable)     // 取前缀 true

// 分组
itertools.groupby(iterable, key=nil)
// → (key, sub_iterator) 对；连续相同 key 的元素分为一组

// 链接/打平
itertools.chain(*iterables)                  // 顺序链接多个迭代器
itertools.chain.from_iterable(iterable)      // chain([iter1, iter2, ...])

// 其他
itertools.zip_longest(*iterables, fillvalue=nil) // 最长 zip，短的补 fillvalue
itertools.accumulate(iterable, func=add, initial=nil) // 前缀累计（默认求和）
itertools.tee(iterable, n=2)                // 复制迭代器为 n 个独立副本
itertools.batched(iterable, n)             // 每 n 个元素一批（Python 3.12+）
```

---

## 实现要点

```c
// groupby：需要缓存 current key 和 current group iterator
// 每次推进 outer iterator 时丢弃前一组的剩余元素
// key 默认 identity（元素本身）

typedef struct MsGroupByObj {
  MsObject header;
  MsValue  srcIter;
  MsValue  keyFunc;
  MsValue  curKey;
  MsValue  curValue;  // 预读一个
  bool     hasValue;
} MsGroupByObj;

// chain.from_iterable：惰性，每次内层 iterator 耗尽再推进外层

// accumulate：保存 running total，每步 func(total, next)

// tee：共享底层迭代器 + 各自的 deque 缓冲
// 任意一个 tee 推进时，其他 tee 的缓冲会被填充

// batched：每次收集 n 个元素返回 tuple（最后一批可能不足 n）
```

---

## 验收标准（checklist）

- [ ] `list(compress("ABCDE", [1,0,1,0,1]))` → `["A","C","E"]`。
- [ ] `list(dropwhile(lambda x: x<5, [1,4,6,3,9]))` → `[6,3,9]`。
- [ ] `groupby` 按连续键分组（非排序）。
- [ ] `list(accumulate([1,2,3,4]))` → `[1,3,6,10]`。
- [ ] `list(zip_longest([1,2],[3],[4,5,6], fillvalue=0))` 正确补齐。
- [ ] `tee(iter, 3)` → 3 个独立迭代器，互不干扰。

---

## 测试用例（.ms）

```ms
import itertools

// groupby（需先排序，groupby 只分连续组）
data := sorted([("a",1),("b",2),("a",3),("b",4)], key=lambda x: x[0])
for key, group in itertools.groupby(data, key=lambda x: x[0]) {
    print(key, list(group))
}
// a [("a",1),("a",3)]
// b [("b",2),("b",4)]

// chain
print(list(itertools.chain([1,2],[3,4],[5])))  // [1,2,3,4,5]

// accumulate（乘积）
import functools
print(list(itertools.accumulate([1,2,3,4,5], functools.mul)))
// [1,2,6,24,120]

// zip_longest
print(list(itertools.zip_longest("AB","xyz", fillvalue="-")))
// [("A","x"),("B","y"),("-","z")]

// tee
a, b := itertools.tee(range(5))
print(list(a))  // [0,1,2,3,4]
print(list(b))  // [0,1,2,3,4]（独立副本）

// batched
print(list(itertools.batched(range(7), 3)))
// [(0,1,2),(3,4,5),(6,)]
```
