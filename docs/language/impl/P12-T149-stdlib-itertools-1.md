# P12-T149 stdlib: itertools（无限迭代器）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `itertools` 模块的无限迭代器：`count`、`cycle`、`repeat`（对齐 `stdlib/itertools.md`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T065 | 迭代协议 GET_ITER/FOR_ITER |

---

## API 清单

```ms
// 无限迭代器（搭配 islice 或手动 break 使用）
itertools.count(start=0, step=1)    // → iterator: start, start+step, ...
itertools.cycle(iterable)           // → iterator: 无限循环 iterable
itertools.repeat(object, times=nil) // → iterator: 重复 object（times=nil=无限）

// 辅助（本任务顺带实现，下一任务详细版）
itertools.islice(iterable, stop)
itertools.islice(iterable, start, stop, step=1)  // 切片迭代器

// 使用示例（不独立成 API）：
// take(n, iter)：取前 n 个
```

---

## 实现要点

```c
// MsCountObj：current + step，tpNext 直接加法
typedef struct MsCountObj {
  MsObject header;
  MsValue  current;
  MsValue  step;
} MsCountObj;

// MsCycleObj：缓存 iterable 元素到 list，然后循环索引
typedef struct MsCycleObj {
  MsObject  header;
  MsListObj* saved;   // 已缓存元素
  MsValue    srcIter; // 原始迭代器（为 nil 时已缓存完毕）
  uint32_t   idx;     // 当前索引
} MsCycleObj;

// MsRepeatObj：object + remaining（-1=无限）
typedef struct MsRepeatObj {
  MsObject header;
  MsValue  object;
  int64_t  remaining;   // -1 = 无限
} MsRepeatObj;

// MsISliceObj：包裹 srcIter + 计数器
// next 时跳过 start 个，每隔 step 个返回一个，到 stop 停止
```

---

## 验收标准（checklist）

- [ ] `list(islice(count(10), 5))` → `[10,11,12,13,14]`。
- [ ] `list(islice(cycle("ABC"), 7))` → `["A","B","C","A","B","C","A"]`。
- [ ] `list(repeat(0, 3))` → `[0,0,0]`。
- [ ] `repeat(x)` 无 times 参数时无限，需 break/islice 截断。
- [ ] `count(start=0, step=0.5)` 支持浮点步长。

---

## 测试用例（.ms）

```ms
import itertools

// count + islice
first10 := list(itertools.islice(itertools.count(1), 10))
print(first10)  // [1,2,3,4,5,6,7,8,9,10]

// cycle
it := itertools.cycle([1,2,3])
result := []
for i in range(7) { result.append(next(it)) }
print(result)   // [1,2,3,1,2,3,1]

// repeat
print(list(itertools.repeat("x", 4)))  // ["x","x","x","x"]

// 组合：生成累积和（每步 count 步长）
it2 := itertools.count(0, 2)
evens := list(itertools.islice(it2, 5))
print(evens)    // [0,2,4,6,8]
```

---

## Benchmark

```ms
import itertools, time
n := 10_000_000
it := itertools.count()
t0 := time.now()
for i in itertools.islice(it, n) { pass }
t1 := time.now()
print("10M count iter:", t1-t0, "ms")  // 目标 < 500ms
```
