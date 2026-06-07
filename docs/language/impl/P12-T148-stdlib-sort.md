# P12-T148 stdlib: sort（Timsort）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `sort` 模块（对齐 `stdlib/sort.md`），提供独立排序算法函数，核心使用 **Timsort**（Python list.sort 同款算法）。同时 `list.sort()` 方法也切换到此实现。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T059 | MsListObj（sort 操作 list） |
| P8-T099 | sorted() 内置函数（调用此模块） |

---

## API 清单

```ms
// sort 模块 — 高级排序接口
sort.sort(list, key=nil, reverse=false)      // 原地排序（Timsort）
sort.sorted(iterable, key=nil, reverse=false) → list  // 返回新列表
sort.isSorted(list, key=nil)                 → bool   // 判断是否已排序
sort.isSortedReverse(list, key=nil)          → bool
sort.stableSort(list, key=nil, reverse=false)  // 显式稳定排序（同 sort，Timsort 稳定）
sort.searchSorted(a, v, side="left") → int   // 二分定位（同 bisect）

// 比较器排序（提供 less 函数）
sort.sortWith(list, less)        // less(a,b) → bool 原地排序
sort.sortedWith(iterable, less) → list

// 特化快速路径（全为 int/float/str 时跳过 MsValue 比较）
sort.sortInts(list)
sort.sortFloats(list)
sort.sortStrings(list)
```

---

## 实现要点

```c
// Timsort 参数：MIN_MERGE = 32（run 长度阈值）
// 1. 扫描自然运行（ascending / descending）
// 2. 短于 MIN_MERGE 的用 binary insertion sort 延伸
// 3. 归并相邻 run（按 galloping mode 优化）
// 4. 合并栈不变式：len[i] > len[i+1] + len[i+2]

// key 函数：预计算所有 key 值（DSU = Decorate-Sort-Undecorate）
// 避免每次比较都调用 key 函数

// 特化路径：若 list 全为 INT tag，直接比较 int64_t（快 3~5×）

static void msTimsort(MsValue* arr, int n, MsValue key, bool rev, MsThread* t);

// gallop_left / gallop_right：指数跳跃加速归并中的二分搜索

// list.sort() 方法链接到此实现（T059 中预留 tpSort 槽）
```

---

## 验收标准（checklist）

- [ ] `sort.sort([3,1,2])` → `[1,2,3]`（原地）。
- [ ] `sort.sorted("banana")` → `["a","a","a","b","n","n"]`（字母排序）。
- [ ] `sort.sort(lst, key=lambda x: -x)` 实现降序。
- [ ] 稳定性：等值元素保持原相对顺序。
- [ ] 已排序 list `sort.isSorted([1,2,3])` → `true`。
- [ ] `sort.sortInts` 比通用 sort 快 3× 以上（benchmark 验证）。

---

## 测试用例（.ms）

```ms
import sort

// 基础
a := [3,1,4,1,5,9,2,6]
sort.sort(a)
print(a)     // [1,1,2,3,4,5,6,9]

// key 函数
words := ["banana","apple","cherry","date"]
sort.sort(words, key=len)
print(words)  // ["date","apple","banana","cherry"]（按长度，同长度稳定）

// 反转
b := list(range(10))
sort.sort(b, reverse=true)
print(b)     // [9,8,...,0]

// 稳定性验证
pairs := [(1,"b"),(2,"a"),(1,"a"),(2,"b")]
sort.sort(pairs, key=lambda p: p[0])
print(pairs)  // [(1,"b"),(1,"a"),(2,"a"),(2,"b")]（稳定：1,"b" 在 1,"a" 前）
```

---

## Benchmark

```ms
import sort, time, random

n := 100_000
lst := [random.randint(0,1000000) for _ in range(n)]

t0 := time.now()
sort.sort(lst)
t1 := time.now()
print("Timsort 100K ints:", t1-t0, "ms")  // 目标 < 50ms

// 特化路径 vs 通用路径
lst2 := list(range(n, 0, -1))
t0 = time.now()
sort.sortInts(lst2)
t1 = time.now()
print("sortInts 100K:", t1-t0, "ms")      // 目标 < 20ms
```
