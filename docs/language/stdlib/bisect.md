# bisect — 有序列表的二分查找与插入

```ms
import bisect
```

## 概述

维护有序列表而无需每次重新排序。所有函数假定列表已按升序排列；若列表无序，
结果未定义。适用于需要频繁插入并保持有序状态的场景，插入代价为 O(log n)（查找）
加 O(n)（列表元素移动）。

## 常量与类型

本模块不定义常量或类型，仅提供操作普通 `list` 的函数。

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `bisect_left` | `bisect_left(lst, x, lo=0, hi=nil, key=nil) → int` | 返回最左插入点 |
| `bisect_right` | `bisect_right(lst, x, lo=0, hi=nil, key=nil) → int` | 返回最右插入点 |
| `bisect` | `bisect(lst, x, lo=0, hi=nil, key=nil) → int` | `bisect_right` 的别名 |
| `insort_left` | `insort_left(lst, x, lo=0, hi=nil, key=nil)` | 插入到最左位置，原地修改 |
| `insort_right` | `insort_right(lst, x, lo=0, hi=nil, key=nil)` | 插入到最右位置，原地修改 |
| `insort` | `insort(lst, x, lo=0, hi=nil, key=nil)` | `insort_right` 的别名 |

## 详细语义

### bisect_left

```
bisect.bisect_left(lst, x, lo=0, hi=nil, key=nil) → int
```

在有序列表 `lst` 中查找 `x` 的最左插入点 `i`，使得将 `x` 插入 `lst[i]` 后列表仍有序。

若 `lst` 中已存在等于 `x` 的元素，`i` 指向最左侧等值元素的位置（即插入后新 `x`
位于所有旧等值元素的**左侧**）。

- `lo`、`hi`：限制搜索范围为 `lst[lo:hi]`，默认搜索整个列表。
- `key`：单参数函数，从列表元素中提取比较键；`x` 直接参与比较，不经过 `key`。
  列表须按 `key(element)` 值有序。

### bisect_right / bisect

```
bisect.bisect_right(lst, x, lo=0, hi=nil, key=nil) → int
bisect.bisect(lst, x, lo=0, hi=nil, key=nil) → int
```

与 `bisect_left` 相同，但返回最右插入点：若已存在等值元素，
新 `x` 将插入所有旧等值元素的**右侧**。

`bisect` 是 `bisect_right` 的别名。

### insort_left

```
bisect.insort_left(lst, x, lo=0, hi=nil, key=nil)
```

将 `x` 插入有序列表 `lst` 中，保持有序。等效于 `lst.insert(bisect_left(...), x)`，
但内部仅一次二分查找。若存在等值元素，`x` 插入其**左侧**。原地修改，无返回值。

### insort_right / insort

```
bisect.insort_right(lst, x, lo=0, hi=nil, key=nil)
bisect.insort(lst, x, lo=0, hi=nil, key=nil)
```

同 `insort_left`，但 `x` 插入等值元素的**右侧**。
`insort` 是 `insort_right` 的别名。

### left 与 right 的区别

对于列表中不存在等值元素的情况，`bisect_left` 和 `bisect_right` 返回相同位置。
区别仅在等值元素存在时：

```
lst = [1, 2, 2, 3]
bisect_left(lst, 2)   → 1  （插入后：[1, 2, 2, 2, 3]，新元素在左）
bisect_right(lst, 2)  → 3  （插入后：[1, 2, 2, 2, 3]，新元素在右）
```

`bisect_left` 通常用于查找（判断元素是否存在）；`bisect_right` 通常用于插入
（确保稳定排序）。

## 示例

```ms
import bisect

// 成绩等级查找表
breakpoints := [60, 70, 80, 90]
grades := ["F", "D", "C", "B", "A"]

func grade(score) {
    i := bisect.bisect(breakpoints, score)
    return grades[i]
}

fmt.println(grade(55))   // F
fmt.println(grade(70))   // C
fmt.println(grade(85))   // B
fmt.println(grade(100))  // A

// 维护有序分数列表
scores := [40, 60, 70, 85]
bisect.insort(scores, 75)
fmt.println(scores)  // [40, 60, 70, 75, 85]

// 判断元素是否在列表中（已排序）
func in_sorted(lst, x) {
    i := bisect.bisect_left(lst, x)
    return i < len(lst) && lst[i] == x
}
fmt.println(in_sorted(scores, 75))  // true
fmt.println(in_sorted(scores, 80))  // false

// 使用 key 参数（列表按元素长度排序）
words := ["hi", "foo", "hello", "world!"]
// 按长度升序：["hi", "foo", "hello", "world!"]
i := bisect.bisect_left(words, 5, key=func(w) { return len(w) })
fmt.println(i)  // 2（插入长度为 5 的词应在 index 2）
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `TypeError` | 列表元素或 `x` 不支持 `<` 比较 |
| `ValueError` | `lo` 或 `hi` 超出合法范围（`lo < 0` 或 `hi > len(lst)`） |
