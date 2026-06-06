# P12-T146 stdlib: array / bisect

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `array` 模块（类型化数组，对齐 `stdlib/array.md`）和 `bisect` 模块（二分查找，对齐 `stdlib/bisect.md`）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T059 | MsListObj（array 参考） |
| P4-T058 | MsBytesObj |

---

## API 清单

```ms
// array（类型化定长元素数组）
a := array.array("i", [1,2,3,4])  // 'b','B','h','H','i','I','l','L','q','Q','f','d'
a.typecode          // → "i"
a.itemsize          // → 4（字节数）
a.append(5)
a.extend(iterable)
a.insert(i, x)
a.pop(i=-1) → value
a.remove(x)         // 删除首次出现
a.index(x)
a.count(x)
a.reverse()
a.tolist() → list
a.frombytes(buf)    // 从 bytes 追加元素
a.tobytes() → bytes
a.fromfile(f, n)    // 从文件读 n 个元素
a.tofile(f)
len(a), a[i], a[i:j]
// 支持 for-in 迭代

// bisect（二分查找，始终保持有序列表不变）
bisect.bisect_left(a, x, lo=0, hi=nil)  → int  // 左侧插入点
bisect.bisect_right(a, x, lo=0, hi=nil) → int  // 右侧插入点（默认）
bisect.bisect(a, x)   // 同 bisect_right
bisect.insort_left(a, x)   // 插入并保持排序（原地）
bisect.insort_right(a, x)
bisect.insort(a, x)   // 同 insort_right
```

---

## 实现要点

```c
// array typecodes 对应 C 类型：
// 'b'→int8_t 'B'→uint8_t 'h'→int16_t 'H'→uint16_t
// 'i'→int32_t 'I'→uint32_t 'l'→int64_t 'L'→uint64_t
// 'q'→int64_t 'Q'→uint64_t 'f'→float 'd'→double

typedef struct MsArrayObj {
    MsObject header;
    char      typecode;   // 'i','d' 等
    uint8_t   itemsize;   // 元素字节数
    uint8_t*  data;       // 原始字节缓冲
    uint32_t  len;        // 当前元素数
    uint32_t  cap;        // 容量（元素数）
} MsArrayObj;

// 读写：自动在 MsValue(INT/FLOAT) 与原始字节间转换
// 下标访问：O(1) 直接指针偏移

// bisect_left 使用标准二分：
// lo=0, hi=len(a); while lo<hi: mid=(lo+hi)//2; if a[mid]<x: lo=mid+1 else: hi=mid
// 通过 msValueLt(a[mid], x) 调用类型比较
```

---

## 验收标准（checklist）

- [ ] `array.array("d", [1.0, 2.0])` 存储为 IEEE 754 double。
- [ ] `a.tobytes()` → 8 字节（2 个 double）。
- [ ] `bisect.bisect_left([1,3,5,7], 5)` → `2`。
- [ ] `bisect.insort([1,3,5], 4)` → `[1,3,4,5]`。
- [ ] array 支持 slice `a[1:3]` 返回新 array。
- [ ] typecode 不支持则抛 ValueError。

---

## 测试用例（.ms）

```ms
import array, bisect

a := array.array("i", range(10))
print(a.tolist())       // [0,1,...,9]
print(a[3:6].tolist())  // [3,4,5]
a.append(100)
a.reverse()
print(a.tolist())       // [100,9,8,...,0]

// bisect
sorted_list := [1,3,5,7,9]
pos := bisect.bisect_left(sorted_list, 6)
print(pos)              // 3
bisect.insort(sorted_list, 6)
print(sorted_list)      // [1,3,5,6,7,9]

// 用 bisect 实现高效等级判断
grades := [60,70,80,90,100]
marks  := ["F","D","C","B","A"]
score := 75
i := bisect.bisect(grades, score)
print(marks[i])         // C
```

---

## Benchmark

```ms
import array, time
n := 1_000_000
a := array.array("d")
t0 := time.now()
for i in range(n) { a.append(float(i)) }
t1 := time.now()
print("1M double append:", t1-t0, "ms")  // 目标 < 200ms
```
