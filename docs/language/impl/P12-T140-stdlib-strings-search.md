# P12-T140 stdlib: strings（查找 / 分割 / 连接）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `strings` 模块：字符串查找、分割、连接操作。这些功能部分已在 `str` 类型的方法上实现（T057），`strings` 模块提供函数式接口和额外功能。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T057 | MsStrObj 方法（str.find/split/join 等） |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-strings-search.md` | §1 模块 API |

---

## API 清单

```ms
// 查找（对齐 stdlib/strings.md）
strings.contains(s, sub)       → bool
strings.count(s, sub)          → int（不重叠出现次数）
strings.find(s, sub, start=0)  → int（首次出现位置，-1=未找到）
strings.rfind(s, sub, end=-1)  → int（从右查找）
strings.index(s, sub)          → int（找不到抛 ValueError）
strings.rindex(s, sub)         → int
strings.startsWith(s, prefix)  → bool（支持 tuple 前缀）
strings.endsWith(s, suffix)    → bool

// 分割
strings.split(s, sep=nil, maxsplit=-1) → list[str]
strings.rsplit(s, sep=nil, maxsplit=-1) → list[str]
strings.splitlines(s, keepends=false)  → list[str]
strings.partition(s, sep)      → (before, sep, after)
strings.rpartition(s, sep)     → (before, sep, after)

// 连接
strings.join(sep, iterable)    → str
strings.concat(*strs)          → str

// 去除空白
strings.strip(s, chars=nil)    → str
strings.lstrip(s, chars=nil)   → str
strings.rstrip(s, chars=nil)   → str

// 大小写
strings.upper(s)   → str
strings.lower(s)   → str
strings.title(s)   → str
strings.swapcase(s) → str
strings.capitalize(s) → str

// 对齐/填充
strings.ljust(s, width, fillchar=" ") → str
strings.rjust(s, width, fillchar=" ") → str
strings.center(s, width, fillchar=" ") → str
strings.zfill(s, width) → str

// 字符测试
strings.isalpha(s)   strings.isdigit(s)   strings.isalnum(s)
strings.isspace(s)   strings.isupper(s)   strings.islower(s)
strings.isprintable(s)  strings.isascii(s)
strings.isidentifier(s)
```

---

## 实现要点

```c
// 大部分为 str 方法的函数式包装
// strings.split(sep=nil) → 按空白分割（连续空白当一个），去除首尾空白
// strings.split(sep="x") → 按精确 sep 分割（不合并连续 sep）

// strings.count 使用 KMP 算法，O(n+m)
// strings.splitlines：识别 \n \r\n \r \x0b \x0c \x1c \x1d \x1e \x85    
```

---

## 验收标准（checklist）

- [ ] `strings.split("a  b  c")` → `["a","b","c"]`（按空白）。
- [ ] `strings.split("a::b::c", "::")` → `["a","b","c"]`。
- [ ] `strings.partition("a:b:c", ":")` → `("a",":",  "b:c")`。
- [ ] `strings.count("aaa", "aa")` → `1`（不重叠）。
- [ ] `strings.join(", ", ["a","b","c"])` → `"a, b, c"`。

---

## 测试用例（.ms）

```ms
import strings

print(strings.split("hello world"))    // ["hello", "world"]
print(strings.split("a::b::c", "::")) // ["a", "b", "c"]
print(strings.join(", ", ["x","y","z"])) // x, y, z
print(strings.count("banana", "an"))   // 2（"an" 出现 2 次）
print(strings.partition("foo:bar:baz", ":"))  // ("foo", ":", "bar:baz")
print(strings.upper("hello"))  // HELLO
print(strings.zfill("42", 5)) // 00042
```

---

## Benchmark

```ms
import strings, time
n := 100_000
s := "hello world " * 100
t0 := time.now()
for i in range(n) { strings.split(s) }
t1 := time.now()
print("100K split:", t1-t0, "ms")  // 目标 < 1s
```
