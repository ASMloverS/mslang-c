# P12-T144 stdlib: collections（deque / Counter）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `collections` 模块的 `deque`（双端队列）和 `Counter`（计数器）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T060 | MsMapObj（Counter 基于 map） |
| P4-T065 | 迭代协议 |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-collections-1.md` | §1 模块 API |

---

## API 清单

```ms
// deque（对齐 stdlib/collections.md）
collections.deque(iterable=(), maxlen=nil)
d.appendleft(x)     // O(1) 左端追加
d.append(x)         // O(1) 右端追加
d.popleft()         // O(1)
d.pop()             // O(1)
d.extendleft(iter)  // 逐个 appendleft
d.extend(iter)
d.rotate(n=1)       // 旋转（正=右旋，负=左旋）
d.reverse()
d[i]                // O(n)（非随机访问）
len(d), bool(d)
d.maxlen            // 最大长度（若设置）
d.clear()
d.copy()

// Counter
collections.Counter(iterable=())
collections.Counter({a:3, b:2, c:1})
c["key"]            // → int（不存在返回 0）
c.mostCommon(n=nil) // → [(elem, count)] 降序
c.elements()        // → 迭代器（每元素重复 count 次）
c.subtract(other)   // 减少计数（可负）
c.update(other)     // 增加计数
c.total()           // → int（所有计数之和）
+c  -c              // 过滤正/负计数
c + c2  c - c2      // Counter 加减
```

---

## 实现要点

```c
// deque：循环缓冲区（ring buffer）
// appendleft/append/popleft/pop 均 O(1)
// 超出 maxlen 时自动从另一端丢弃
typedef struct MsDequeObj {
  MsObject header;
  MsValue* buf;         // 循环缓冲区
  uint32_t cap;         // 容量（总是 2 的幂）
  uint32_t head;        // 左端索引
  uint32_t len;         // 当前元素数
  int64_t  maxlen;      // -1 = 无限制
} MsDequeObj;

// Counter：继承 map（MsMapObj），默认返回 0
// Counter[key] → __missing__ 返回 0（而非 KeyError）
```

---

## 验收标准（checklist）

- [ ] `deque([1,2,3])` → 可左右追加/弹出。
- [ ] `deque(maxlen=3)` 超出时自动丢弃旧元素。
- [ ] `Counter("banana")` → `{"b":1,"a":3,"n":2}`。
- [ ] `c.mostCommon(2)` 返回前 2 个高频元素。
- [ ] `Counter("ab") + Counter("bc")` → `{"a":1,"b":2,"c":1}`。

---

## 测试用例（.ms）

```ms
import collections

// deque
d := collections.deque([1,2,3])
d.appendleft(0)
d.append(4)
print(list(d))    // [0,1,2,3,4]
d.rotate(1)
print(list(d))    // [4,0,1,2,3]

// deque maxlen
d2 := collections.deque(maxlen=3)
for i in range(5) { d2.append(i) }
print(list(d2))   // [2,3,4]（前两个被丢弃）

// Counter
c := collections.Counter("banana")
print(c["a"])     // 3
print(c.mostCommon(2))  // [("a",3),("n",2)]

c2 := collections.Counter("apple")
print((c + c2).mostCommon(3))
```
