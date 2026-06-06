# P12-T150 stdlib: itertools（组合迭代器）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `itertools` 的组合类迭代器：`product`、`permutations`、`combinations`、`combinations_with_replacement`。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T149 | itertools 无限迭代器 |

---

## API 清单

```ms
// 组合迭代器（笛卡尔积/排列/组合）
itertools.product(*iterables, repeat=1)
// 笛卡尔积，等同 nested for-loops
// product([1,2],["a","b"]) → (1,"a"),(1,"b"),(2,"a"),(2,"b")

itertools.permutations(iterable, r=nil)
// 全排列（r=nil 则取所有位置）
// permutations("ABC",2) → AB,AC,BA,BC,CA,CB

itertools.combinations(iterable, r)
// 无重复组合（字典序）
// combinations("ABC",2) → AB,AC,BC

itertools.combinations_with_replacement(iterable, r)
// 有重复组合
// combinations_with_replacement("AB",2) → AA,AB,BB

// 辅助
itertools.pairwise(iterable)   // → (a,b),(b,c),(c,d) 相邻对（Python 3.10+）
itertools.starmap(func, iterable)  // starmap(pow, [(2,5),(3,2)]) → 32,9
```

---

## 实现要点

```c
// product：多指针法（indices 数组，每步从右向左进位）
typedef struct MsProductObj {
    MsObject   header;
    MsListObj* pools[];   // 各 iterable 的缓存
    uint32_t   npools;
    uint32_t*  indices;   // 当前各维度索引
    bool       done;
} MsProductObj;

// permutations：Heap's 算法或 Knuth 下一个排列
// 状态：indices + cycles 数组（Knuth P 算法）
// 每次 tp_next 推进一步

// combinations：indices 数组，按字典序递增
// 每次找最右可增位，右侧全置为连续递增

// combinations_with_replacement：类似 combinations，
// 差异：indices[i] >= indices[i-1]（而非 >）

// 注意：全部迭代器需 GC 标记内部 pools/indices
```

---

## 验收标准（checklist）

- [ ] `list(product([0,1], repeat=3))` → 8 个三元组（00 → 11 全部二进制组合）。
- [ ] `list(permutations("ABC"))` 共 6 个，顺序正确。
- [ ] `list(combinations("ABCD", 2))` 共 C(4,2)=6 个。
- [ ] `list(combinations_with_replacement("AB", 2))` → `[("A","A"),("A","B"),("B","B")]`。
- [ ] `starmap(lambda a,b: a**b, [(2,3),(3,2)])` → `[8,9]`。

---

## 测试用例（.ms）

```ms
import itertools

// product
print(list(itertools.product([1,2],[3,4])))
// [(1,3),(1,4),(2,3),(2,4)]

// repeat 参数
print(list(itertools.product("AB", repeat=2)))
// [("A","A"),("A","B"),("B","A"),("B","B")]

// permutations
perms := list(itertools.permutations([1,2,3]))
print(len(perms))  // 6
print(perms[0])    // (1,2,3)

// combinations
combs := list(itertools.combinations("ABCD", 2))
print(len(combs))  // 6

// combinations_with_replacement
cwr := list(itertools.combinations_with_replacement("AB", 3))
print(len(cwr))    // 4: AAA,AAB,ABB,BBB

// pairwise
print(list(itertools.pairwise([1,2,3,4])))
// [(1,2),(2,3),(3,4)]
```
