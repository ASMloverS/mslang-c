# copy — 浅拷贝与深拷贝

```ms
import copy
```

## 概述

创建对象副本。浅拷贝（shallow copy）复制对象本身但共享嵌套引用；深拷贝
（deep copy）递归复制所有嵌套对象。正常赋值（`b := a`）只复制引用；当需要
独立副本时使用本模块。

## 常量与类型

本模块不定义常量或类型。

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `copy` | `copy.copy(obj) → objCopy` | 浅拷贝 |
| `deepcopy` | `copy.deepcopy(obj, memo=nil) → objCopy` | 深拷贝 |
| `replace` | `copy.replace(obj, **changes) → objCopy` | 带字段覆盖的浅拷贝 |

## 详细语义

### copy.copy — 浅拷贝

```
copy.copy(obj) → objCopy
```

**不同类型的浅拷贝行为：**

| 类型 | 行为 |
|---|---|
| `int`、`float`、`bool`、`nil`、`str` | 返回原对象（不可变，无需复制） |
| `bytes`、`frozenset`、`tuple` | 返回原对象（不可变） |
| `list` | 新列表，元素引用共享 |
| `set` | 新集合，元素引用共享 |
| `map` | 新映射，键值引用共享 |
| 自定义 class | 优先调用 `obj.__copy__()`；否则创建新实例并浅拷贝所有属性 |

浅拷贝只复制一层：嵌套的 list、map 或对象**不会**被复制，修改嵌套内容会影响
原对象：

```ms
a := [[1, 2], [3, 4]]
b := copy.copy(a)
b[0].append(99)
fmt.println(a[0])  // [1, 2, 99]（a 受影响）
fmt.println(b[0])  // [1, 2, 99]（b[0] 与 a[0] 是同一对象）
```

### copy.deepcopy — 深拷贝

```
copy.deepcopy(obj, memo=nil) → objCopy
```

递归复制对象及其所有嵌套对象。`memo` 是一个 map，用于跟踪已复制的对象以处理
循环引用；通常传 `nil`（由函数内部自动创建）。

**循环引用处理：** 若对象 A 包含对自身的引用，深拷贝会检测到（通过 `memo`
记录对象 id），在副本中重建相同的循环结构，而非陷入无限递归。

**不同类型的深拷贝行为：**

| 类型 | 行为 |
|---|---|
| `int`、`float`、`bool`、`nil`、`str` | 返回原对象（不可变） |
| `bytes`、`frozenset` | 返回原对象（不可变） |
| `tuple` | 若所有元素均不可变则返回原对象；否则递归深拷贝元素后创建新 tuple |
| `list`、`set`、`map` | 创建新容器，递归深拷贝所有元素 |
| 自定义 class | 优先调用 `obj.__deepcopy__(memo)`；否则创建新实例并递归深拷贝所有属性 |

深拷贝后修改副本的任意嵌套内容**不会**影响原对象：

```ms
a := [[1, 2], [3, 4]]
b := copy.deepcopy(a)
b[0].append(99)
fmt.println(a[0])  // [1, 2]（a 不受影响）
fmt.println(b[0])  // [1, 2, 99]
```

### copy.replace — 带覆盖的副本

```
copy.replace(obj, **changes) → objCopy
```

创建 `obj` 的浅拷贝，同时用 `changes` 中的键值覆盖对应属性。要求对象实现
`__replace__(**changes)` 方法（命名元组风格对象的标准协议）。

```ms
class Point {
    func __init__(self, x, y) {
        self.x = x
        self.y = y
    }
    func __replace__(self, **changes) {
        x := changes.get("x", self.x)
        y := changes.get("y", self.y)
        return Point(x, y)
    }
}

p1 := Point(1, 2)
p2 := copy.replace(p1, x=10)
fmt.println(p1.x, p1.y)  // 1 2
fmt.println(p2.x, p2.y)  // 10 2
```

### 自定义拷贝行为

实现魔法方法以控制拷贝逻辑：

**`__copy__(self) → copy`**

由 `copy.copy()` 调用。返回 `self` 的浅拷贝。

**`__deepcopy__(self, memo) → copy`**

由 `copy.deepcopy()` 调用。在处理子对象时应传递 `memo`：

```ms
class Tree {
    func __init__(self, val, children=nil) {
        self.val = val
        self.children = children if children != nil else []
    }
    func __deepcopy__(self, memo) {
        newNode := Tree(self.val)
        memo[id(self)] = newNode          // 先注册，再递归，防止循环
        newNode.children = copy.deepcopy(self.children, memo)
        return newNode
    }
}
```

**不可拷贝对象：** 某些系统资源（打开的文件句柄、锁对象、网络连接）无法有意义
地复制，调用时抛 `TypeError`。

## 示例

```ms
import copy
import fmt

// 1. 浅拷贝 vs 深拷贝
original := {"a": [1, 2, 3], "b": [4, 5, 6]}

shallow := copy.copy(original)
deep    := copy.deepcopy(original)

original["a"].append(99)

fmt.println(shallow["a"])   // [1, 2, 3, 99]（共享引用，受影响）
fmt.println(deep["a"])      // [1, 2, 3]（独立副本，不受影响）

// 2. 自定义类的 __copy__ 与 __deepcopy__
class Config {
    func __init__(self, settings, cache) {
        self.settings = settings
        self.cache    = cache
    }
    func __copy__(self) {
        return Config(self.settings, self.cache)  // 浅：共享 settings 和 cache
    }
    func __deepcopy__(self, memo) {
        return Config(
            copy.deepcopy(self.settings, memo),
            {},  // cache 刻意不复制，始终从空缓存开始
        )
    }
}

cfg   := Config({"timeout": 30}, {"key": "cached_val"})
cfg2  := copy.copy(cfg)
cfg3  := copy.deepcopy(cfg)

fmt.println(cfg2.cache is cfg.cache)    // true（浅拷贝共享 cache）
fmt.println(cfg3.cache)                 // {}（深拷贝 cache 被重置）

// 3. 循环引用
a := [1, 2]
a.append(a)   // a 引用自身：[1, 2, [...]]
b := copy.deepcopy(a)
fmt.println(b[2] is b)   // true（循环结构被正确重建）
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `TypeError` | 对象不支持拷贝（如打开的文件句柄、锁对象） |
| `TypeError` | `copy.replace` 的对象未实现 `__replace__` |
