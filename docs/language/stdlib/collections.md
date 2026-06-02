# collections — 高性能容器数据类型

```ms
import collections
```

## 概述

提供超越内置 `list`/`map` 的专用数据结构，包括双端队列、计数器、带默认值映射、
有序映射和命名元组。各类型参考 Python `collections` 模块语义，针对 mslang
类型系统做相应适配。

## 常量与类型

| 名称 | 说明 |
|---|---|
| `collections.deque` | 双端队列类 |
| `collections.Counter` | 元素计数器类（map 子类） |
| `collections.defaultdict` | 带默认值工厂的 map 类 |
| `collections.OrderedDict` | 保持插入顺序的 map 类 |
| `collections.namedtuple` | 命名字段元组工厂函数 |

## 函数签名速查

| 名称 | 签名 | 说明 |
|---|---|---|
| `deque` | `deque(iterable=nil, maxlen=nil)` | 构造双端队列 |
| `Counter` | `Counter(iterable_or_map=nil)` | 构造计数器 |
| `defaultdict` | `defaultdict(default_factory, *args)` | 构造带默认工厂的 map |
| `OrderedDict` | `OrderedDict(*args)` | 构造有序 map |
| `namedtuple` | `namedtuple(typename, field_names) → class` | 生成命名元组类 |

## 详细语义

### deque

```
collections.deque(iterable=nil, maxlen=nil)
```

双端队列，头尾插入/删除均为 O(1)。中间位置随机访问为 O(n)。

`maxlen` 为 nil 时容量无限；设置正整数后充当循环缓冲区——当队列已满时，
从相反端插入会自动丢弃最旧的元素（`append` 丢弃左端，`appendleft` 丢弃右端）。

**方法**

| 方法 | 说明 |
|---|---|
| `append(x)` | 向右端添加元素 |
| `appendleft(x)` | 向左端添加元素 |
| `pop()` | 移除并返回右端元素；空时抛 `IndexError` |
| `popleft()` | 移除并返回左端元素；空时抛 `IndexError` |
| `extend(iter)` | 将 iter 中的元素依次追加到右端 |
| `extendleft(iter)` | 将 iter 中的元素依次追加到左端（结果顺序反转） |
| `rotate(n=1)` | 将队列向右旋转 n 步；n 为负则向左旋转 |
| `clear()` | 移除所有元素 |
| `copy()` | 返回浅拷贝 |
| `count(x)` | 返回 x 出现的次数 |
| `remove(x)` | 移除第一个等于 x 的元素；不存在时抛 `ValueError` |
| `index(x)` | 返回第一个等于 x 的下标；不存在时抛 `ValueError` |
| `insert(i, x)` | 在位置 i 处插入 x；若有 maxlen 且已满则抛 `IndexError` |
| `reverse()` | 原地反转 |

**属性**

- `maxlen`：只读，构造时指定的最大长度；无限时为 `nil`。

支持 `len(d)`、`d[i]`（支持负索引）、`for x in d`、`x in d`。

### Counter

```
collections.Counter(iterable_or_map=nil)
```

`map` 的子类，专用于元素计数。键为元素，值为计数（int）。

- 传入可迭代对象：统计各元素出现次数。
- 传入 map：以 map 值作为初始计数。
- 访问不存在的键返回 `0`，不抛 `KeyError`。

**方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `most_common` | `most_common(n=nil) → list` | 返回按计数降序排列的 `(元素, 计数)` 列表；n=nil 返回全部 |
| `elements` | `elements() → iterator` | 返回迭代器，每个元素按其计数重复出现；计数 ≤ 0 的元素被跳过 |
| `update` | `update(other)` | 将 other（可迭代对象或 Counter）的计数累加进来 |
| `subtract` | `subtract(other)` | 将 other 的计数从自身减去（允许结果为负） |
| `total` | `total() → int` | 返回所有计数之和 |

**算术运算**

| 运算符 | 语义 |
|---|---|
| `c1 + c2` | 合并，只保留正计数 |
| `c1 - c2` | 相减，只保留正计数 |
| `c1 \| c2` | 并集：取各键的最大计数 |
| `c1 & c2` | 交集：取各键的最小正计数 |
| `+c` | 过滤，只保留正计数的副本 |
| `-c` | 取反，只保留负计数（取绝对值） |

### defaultdict

```
collections.defaultdict(default_factory, *args)
```

`map` 的子类。当访问不存在的键时，调用 `default_factory()` 生成默认值并存入映射，
然后返回该值。`default_factory` 为 `nil` 时行为与普通 map 相同（抛 `KeyError`）。

`*args` 与 `map()` 构造参数相同，用于初始化内容。

**属性**

- `default_factory`：可读写，无参可调用对象或 `nil`。

### OrderedDict

```
collections.OrderedDict(*args)
```

保持键插入顺序的 map（注：mslang 内置 `map` 已按插入顺序迭代，
`OrderedDict` 在此基础上额外提供顺序感知的操作和相等性语义）。

**方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `move_to_end` | `move_to_end(key, last=true)` | 将 key 移动到末尾（last=true）或开头（last=false）；key 不存在时抛 `KeyError` |
| `popitem` | `popitem(last=true) → (key, value)` | 移除并返回末尾（last=true）或开头（last=false）的键值对；空时抛 `KeyError` |

**相等性**：两个 `OrderedDict` 相等，当且仅当键相同、值相同，且插入顺序也相同。
与普通 `map` 比较时忽略顺序。

### namedtuple

```
collections.namedtuple(typename, field_names) → class
```

返回一个新的 `tuple` 子类，实例具有按名称访问的字段。

- `typename`：新类的名称字符串。
- `field_names`：字段名列表，或以空格/逗号分隔的字符串。
- 字段名不能以下划线开头，不能是关键字，不能重复。

生成类的实例具备：

| 成员 | 说明 |
|---|---|
| `._fields` | 字段名元组（只读） |
| `._asdict()` | 返回字段名到值的 `map` |
| `._replace(**kw)` | 返回替换指定字段后的新实例 |
| `._make(iter)` | 类方法，从可迭代对象构造实例 |

## 示例

```ms
import collections

// deque 作为固定大小的循环缓冲区（最近 3 条日志）
log := collections.deque(maxlen=3)
log.append("a")
log.append("b")
log.append("c")
log.append("d")           // "a" 被自动丢弃
fmt.println(list(log))    // ["b", "c", "d"]

// Counter 统计词频
words := ["apple", "banana", "apple", "cherry", "banana", "apple"]
cnt := collections.Counter(words)
fmt.println(cnt.most_common(2))  // [("apple", 3), ("banana", 2)]
fmt.println(cnt["grape"])        // 0（不存在的键）

// defaultdict 按首字母分组
dd := collections.defaultdict(list)
for word in words {
    dd[word[0]].append(word)
}
fmt.println(dd["a"])  // ["apple", "apple", "apple"]

// namedtuple 定义二维点
Point := collections.namedtuple("Point", ["x", "y"])
p := Point(3, 4)
fmt.println(p.x, p.y)          // 3 4
fmt.println(p._asdict())       // {"x": 3, "y": 4}
p2 := p._replace(y=10)
fmt.println(p2)                // Point(x=3, y=10)
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `IndexError` | `deque.pop` / `popleft` 在空队列上调用；`deque.insert` 在有 maxlen 且已满时调用 |
| `ValueError` | `deque.remove` / `deque.index` 元素不存在；`namedtuple` 字段名非法或重复 |
| `KeyError` | `OrderedDict.move_to_end` / `popitem` 键不存在；`defaultdict` 的 `default_factory` 为 nil 时访问缺失键 |
| `TypeError` | `Counter` 算术运算类型不匹配；`defaultdict.default_factory` 不可调用 |
