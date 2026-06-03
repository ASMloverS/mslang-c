# sort — 排序工具

```ms
import sort
```

## 概述

就地排序与新排序工具，补充内置 `sorted` 函数。参考 Python/Go 排序约定。
**所有排序算法均为稳定排序**——相等元素保持原始相对顺序。

内置 `sorted(lst, key=nil, reverse=false)` 返回新列表；本模块的 `sort.sort`
提供就地排序，并补充二分查找、有序性检查、原地反转等实用操作。

## 常量与类型

本模块不定义常量或类型，仅操作普通 `list`。

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `sort` | `sort.sort(lst, key=nil, reverse=false)` | 就地排序；返回 nil |
| `stable` | `sort.stable(lst, key=nil)` | 显式稳定就地排序（与 sort 相同，强调稳定性） |
| `sorted` | `sort.sorted(lst, key=nil, reverse=false) → list` | 返回新排序列表 |
| `search` | `sort.search(lst, target) → int` | 二分查找，返回插入点；O(log n) |
| `is_sorted` | `sort.is_sorted(lst, key=nil, reverse=false) → bool` | 检查是否已有序；O(n) |
| `reverse` | `sort.reverse(lst)` | 就地反转列表 |

## 详细语义

### sort.sort

```
sort.sort(lst, key=nil, reverse=false)
```

对 `lst` 进行**就地**稳定排序（Timsort），返回 `nil`。

- `key`：单参数函数，从每个元素提取比较键；每个元素仅调用一次。
- `reverse=true`：降序排列（等同于升序后反转，但更高效且保持稳定）。
- 列表元素（或 key 结果）须支持 `<` 运算；否则抛 `TypeError`。

### sort.stable

```
sort.stable(lst, key=nil)
```

与 `sort.sort` 语义完全相同，不提供 `reverse` 参数。适用于代码中需要明确表达
"此处要求稳定排序"意图的场景；两者底层算法一致。

### sort.sorted

```
sort.sorted(lst, key=nil, reverse=false) → list
```

返回新的已排序列表，`lst` 本身不变。与内置 `sorted` 完全等价，此处作为模块内
统一接口提供，便于在 `sort` 命名空间下一致调用。

### sort.search

```
sort.search(lst, target) → int
```

在**已排序**列表 `lst` 中查找 `target` 的最左插入点（等同于 `bisect.bisect_left`）。

- 返回值 `i` 满足：`lst[i-1] < target <= lst[i]`（边界情况：`0` 到 `len(lst)`）。
- 若 `target` 在列表中存在，返回最左匹配位置的索引。
- 若 `target` 不存在，返回应插入的位置（使列表仍有序）。
- **不保证**列表中存在 `target`；调用方需自行检查 `lst[i] == target`。
- 列表**必须已排序**；无序时结果未定义。时间复杂度 O(log n)。

```ms
lst := [10, 20, 30, 40, 50]
i := sort.search(lst, 30)
fmt.println(i)              // 2
fmt.println(lst[i] == 30)   // true（确认存在）

j := sort.search(lst, 25)
fmt.println(j)              // 2（25 应插入 index 2）
fmt.println(lst[j] == 25)   // false（不存在）
```

### sort.is_sorted

```
sort.is_sorted(lst, key=nil, reverse=false) → bool
```

线性扫描列表，检查是否满足排序条件。

- `key`：与 `sort.sort` 相同，从元素提取比较键。
- `reverse=false`：检查升序；`reverse=true`：检查降序。
- 空列表或单元素列表始终返回 `true`。时间复杂度 O(n)。

### sort.reverse

```
sort.reverse(lst)
```

就地反转 `lst` 中所有元素的顺序，返回 `nil`。等价于 `lst.reverse()`（若列表内置
此方法），但作为模块函数提供。时间复杂度 O(n)。

### key 函数协议

`key(item)` 每个元素调用一次，比较基于返回值。key 返回值必须支持 `<` 运算。
使用 `functools.cmp_to_key`（见 `functools` 模块）将旧式三路比较函数转换为 key 函数：

```ms
import sort
import functools

cmp := func(a, b) { return a["priority"] - b["priority"] }
sort.sort(tasks, key=functools.cmp_to_key(cmp))
```

## 示例

```ms
import sort
import fmt

// 1. 基础就地排序
nums := [3, 1, 4, 1, 5, 9, 2, 6]
sort.sort(nums)
fmt.println(nums)   // [1, 1, 2, 3, 4, 5, 6, 9]

// 2. 按字典字段排序
people := [
    {"name": "Alice", "age": 30},
    {"name": "Bob",   "age": 25},
    {"name": "Carol", "age": 35},
]
sort.sort(people, key=func(p) { return p["age"] })
fmt.println(people[0]["name"])  // "Bob"

// 3. 降序排序
sort.sort(nums, reverse=true)
fmt.println(nums)   // [9, 6, 5, 4, 3, 2, 1, 1]

// 4. 不修改原列表
original := [3, 1, 2]
ordered := sort.sorted(original)
fmt.println(original)  // [3, 1, 2]（不变）
fmt.println(ordered)   // [1, 2, 3]

// 5. 二分查找
prices := [10, 20, 30, 40, 50]
idx := sort.search(prices, 30)
if idx < len(prices) && prices[idx] == 30 {
    fmt.println("found at index", idx)  // found at index 2
}

// 6. 检查有序性
fmt.println(sort.is_sorted([1, 2, 3, 4]))    // true
fmt.println(sort.is_sorted([4, 3, 2, 1]))    // false
fmt.println(sort.is_sorted([4, 3, 2, 1], reverse=true))  // true

// 7. 就地反转
items := ["a", "b", "c", "d"]
sort.reverse(items)
fmt.println(items)   // ["d", "c", "b", "a"]

// 8. 稳定性验证（相等元素保持原序）
records := [
    {"key": 1, "order": "first"},
    {"key": 2, "order": "A"},
    {"key": 1, "order": "second"},
]
sort.sort(records, key=func(r) { return r["key"] })
fmt.println(records[0]["order"])  // "first"（key=1 的两条记录保持原序）
fmt.println(records[1]["order"])  // "second"
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `TypeError` | 列表元素（或 key 结果）不支持 `<` 比较 |
| `TypeError` | `key` 不是可调用对象 |
