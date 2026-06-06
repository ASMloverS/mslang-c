# P12-T145 stdlib: collections（defaultdict / OrderedDict / namedtuple）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `collections` 模块的 `defaultdict`、`OrderedDict`、`namedtuple`。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T144 | collections deque/Counter |
| P4-T060 | MsMapObj |
| P5-T072 | 实例化/class |

---

## API 清单

```ms
// defaultdict：访问缺失键时自动调用 default_factory
d := collections.defaultdict(list)
d["x"].append(1)   // 自动创建 d["x"] = []
d["x"].append(2)
print(d["x"])      // [1, 2]
d.default_factory  // → list（可重赋值）

// OrderedDict：保持插入顺序（当前 map 已有序，额外功能）
od := collections.OrderedDict()
od["a"] = 1
od["b"] = 2
od.move_to_end("a")         // 移到末尾
od.move_to_end("b", last=false) // 移到开头
list(od.keys())             // 按顺序
od.popitem(last=true)       // LIFO；false=FIFO

// namedtuple：轻量命名元组（元组子类）
Point := collections.namedtuple("Point", ["x", "y"])
p := Point(1, 2)
print(p.x, p.y)    // 1 2
print(p[0])        // 1（元组索引）
p2 := p._replace(x=10)  // 返回新实例
print(Point._fields)   // ["x", "y"]
print(p._asdict())     // {"x":1, "y":2}
```

---

## 实现要点

```c
// defaultdict：继承 MsMapObj，重写 tp_getitem
// 若键不存在，调用 default_factory() 并存入，然后返回
typedef struct MsDefaultDictObj {
    MsMapObj base;
    MsValue  defaultFactory;  // callable or nil
} MsDefaultDictObj;

// OrderedDict：mslang map 已保序（insertion order），
// 额外实现 move_to_end / popitem(last) 即可
// 内部维护双向链表顺序索引

// namedtuple：动态生成新 MsTypeObj（继承 tuple），
// 为每个字段生成只读属性（通过 __slots__ 机制）
// namedtuple("Point", ["x","y"]) → 等同于定义：
//   class Point(tuple):
//       @property
//       def x(self): return self[0]
//       @property
//       def y(self): return self[1]
```

---

## 验收标准（checklist）

- [ ] `defaultdict(int)["missing"]` → `0`（int() = 0）。
- [ ] `defaultdict(list)["k"].append(1)` 不抛 KeyError。
- [ ] `OrderedDict.move_to_end("a")` 正确改变遍历顺序。
- [ ] `OrderedDict.popitem(last=false)` 返回最早插入的键值对。
- [ ] `namedtuple` 支持索引访问与属性访问。
- [ ] `namedtuple._replace()` 返回新实例，原实例不变。

---

## 测试用例（.ms）

```ms
import collections

// defaultdict
dd := collections.defaultdict(list)
for word in ["apple","banana","avocado","blueberry"] {
    dd[word[0]].append(word)
}
print(dd["a"])  // ["apple", "avocado"]

// OrderedDict
od := collections.OrderedDict()
for c in "cba" { od[c] = ord(c) }
print(list(od.keys()))   // ["c","b","a"]
od.move_to_end("c", last=false)
print(list(od.keys()))   // ["c","b","a"] → ["c" moved to front]

// namedtuple
Vector := collections.namedtuple("Vector", ["x","y","z"])
v := Vector(1, 2, 3)
print(v.x, v.y, v.z)    // 1 2 3
print(v._asdict())       // {"x":1,"y":2,"z":3}
v2 := v._replace(z=10)
print(v2)                // Vector(x=1, y=2, z=10)
```
