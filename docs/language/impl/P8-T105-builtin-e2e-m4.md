# P8-T105 内置函数 .ms 测试套件（M4 里程碑）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

通过完整的 `.ms` 测试套件验证 P8 所有内置函数（T096–T104）。此任务是 P8 阶段的**里程碑收口**（M4）：所有测试通过后，mslang 具备与 Python 相当的内置函数完备性，可用于实际脚本编写。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P8-T096 ~ T104 | P8 所有内置函数 |

---

## M4 测试套件（`tests/ms/p8/`）

### `tests/ms/p8/builtins_basic.ms`

```ms
// 基础内置函数验证
print(len("hello"))     // 5
print(len([1,2,3]))     // 3
print(len({a:1}))       // 1

print(type(42).__name__)     // int
print(type(3.14).__name__)   // float
print(type("x").__name__)    // str
print(type([]).__name__)     // list
print(type({}).__name__)     // map
print(type(()).__name__)     // tuple
print(type(nil).__name__)    // nil

print(repr([1,"a",nil]))   // [1, "a", nil]
print(str(3.14))           // 3.14
```

**期望输出**：
```
5
3
1
int
float
str
list
map
tuple
nil
[1, "a", nil]
3.14
```

### `tests/ms/p8/numeric_builtins.ms`

```ms
// 数值转换
print(int("42"))       // 42
print(int(3.7))        // 3
print(float("3.14"))   // 3.14
print(bool(0))         // false
print(bool("x"))       // true

// abs / round / pow / divmod
print(abs(-5))         // 5
print(round(3.14159, 2))  // 3.14
print(pow(2, 10))      // 1024
q, r := divmod(17, 5)
print(q, r)            // 3 2

// chr / ord / hex / bin / oct
print(chr(65))         // A
print(ord("z"))        // 122
print(hex(255))        // 0xff
print(bin(10))         // 0b1010
print(oct(8))          // 0o10
```

**期望输出**：
```
42
3
3.14
false
true
5
3.14
1024
3 2
A
122
0xff
0b1010
0o10
```

### `tests/ms/p8/iter_builtins.ms`

```ms
// enumerate / zip / map / filter
for i, v in enumerate(["a","b","c"]) {
    print(i, v)
}

for a, b in zip([1,2], [10,20]) {
    print(a * b)
}

doubled := list(map(func(x){ return x*2 }, [1,2,3,4]))
print(doubled)

evens := list(filter(func(x){ return x%2==0 }, range(6)))
print(evens)

// any / all
print(any([0, 0, 1]))   // true
print(all([1, 2, 3]))   // true
print(all([1, 0, 3]))   // false

// sorted / reversed / sum / min / max
print(sorted([3,1,4,1,5]))  // [1, 1, 3, 4, 5]
print(list(reversed([1,2,3])))  // [3, 2, 1]
print(sum(range(101)))   // 5050
print(min(3,1,2))        // 1
print(max([10,20,30]))   // 30
```

**期望输出**：
```
0 a
1 b
2 c
10
40
[2, 4, 6, 8]
[0, 2, 4]
true
true
false
[1, 1, 3, 4, 5]
[3, 2, 1]
5050
1
30
```

### `tests/ms/p8/io_builtins.ms`

```ms
// open / read / write
import tempfile
tmpf := tempfile.mkstemp()

with open(tmpf, "w") as f {
    f.write("line1\nline2\nline3\n")
}
with open(tmpf) as f {
    for line in f {
        print(line.strip())
    }
}

// callable / hash / id
func greet() {}
print(callable(greet))   // true
print(callable(42))      // false

print(hash(42) == hash(42))  // true
x := [1,2]
print(id(x) == id(x))    // true

import os
os.remove(tmpf)
```

---

## 验收标准（checklist）

- [ ] 所有 `tests/ms/p8/*.ms` golden 测试通过。
- [ ] 数值转换（int/float/bool）边界正确处理（NaN、溢出、非法字符串）。
- [ ] 惰性迭代器（enumerate/zip/map/filter）不预取，仅在 for 循环中求值。
- [ ] `sorted` 返回新列表，原列表不变。
- [ ] `open` + with 语句正确关闭文件。
- [ ] 所有内置函数的 `TypeError`/`ValueError` 可被 catch 捕获。

---

## Benchmark（M4 综合）

```ms
// benchmarks/bench_builtins.ms
// 1. sum 大 range
t0 := time.now()
print(sum(range(10_000_000)))   // 49999995000000
t1 := time.now()
print("sum(range(10M)):", t1-t0, "ms")  // 目标 < 2s

// 2. map/filter 链
t0 = time.now()
result := sum(map(func(x){ return x*x },
              filter(func(x){ return x%2==0 }, range(1_000_000))))
print(result)
t1 = time.now()
print("map+filter 1M:", t1-t0, "ms")  // 目标 < 2s

// 3. sorted
import random
lst := list(range(100_000))
// (如果有 random.shuffle) random.shuffle(lst)
t0 = time.now()
sorted(lst)
t1 = time.now()
print("sorted 100K:", t1-t0, "ms")  // 目标 < 200ms
```

---

## 风险与边界

- **M4 定义**：M4 通过 = P8 所有内置函数完备 + benchmark 全部达标。至此 mslang 可用于大多数非并发、非系统级脚本任务。
