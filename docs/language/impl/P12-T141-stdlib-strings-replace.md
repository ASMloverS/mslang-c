# P12-T141 stdlib: strings（替换 / Builder / Unicode）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `strings` 模块的替换、Builder（高效字符串拼接）和 Unicode 分析功能。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T140 | strings 查找/分割 |
| P4-T057 | MsStrObj（UTF-8） |

---

## API 清单

```ms
// 替换
strings.replace(s, old, new, count=-1) → str（count=-1=全部）
strings.maketrans(x, y=nil, z=nil)    → dict（翻译表）
strings.translate(s, table)           → str（应用翻译表）
strings.expandtabs(s, tabsize=8)      → str

// 编码/解码
strings.encode(s, encoding="utf-8")   → bytes
strings.decode(b, encoding="utf-8")   → str
strings.toBytes(s)                    → bytes（UTF-8 编码）
strings.fromBytes(b)                  → str（UTF-8 解码）

// Unicode
strings.unicodeCategory(ch)   → str（"Lu"/"Ll"/"Nd"等）
strings.unicodeName(ch)       → str（Unicode 字符名）
strings.normalizeNFC(s)       → str（NFC 规范化，简化版）
strings.isNormalized(s, form) → bool

// StringBuilder（高效拼接）
sb := strings.Builder()
sb.write(s)          // 追加字符串
sb.writeln(s)        // 追加字符串 + "\n"
sb.writeByte(b)      // 追加单个字节
sb.toString()        // 获取结果字符串
sb.len()             // 当前字节长度
sb.reset()           // 清空

// 格式化
strings.format(template, **kwargs) → str（f-string 风格）
strings.removePrefix(s, prefix)   → str
strings.removeSuffix(s, suffix)   → str
strings.repeat(s, n)              → str
strings.reverseStr(s)             → str（Unicode 安全，反转 codepoints）
```

---

## 实现要点

```c
// StringBuilder：内部用 MsWriter（动态字节缓冲），减少中间字符串分配
// 典型用法：循环 sb.write()，最后一次 sb.toString()
// replace: KMP 查找 + 替换（O(n*m) 最坏情况，但实践中快）

// strings.reverseStr：遍历 codepoints（不是字节）反转
// strings.toBytes("你好") → b'\xe4\xbd\xa0\xe5\xa5\xbd'
```

---

## 验收标准（checklist）

- [ ] `strings.replace("aaa", "a", "b", 2)` → `"bba"`（count=2）。
- [ ] `strings.Builder()` 批量拼接比 `+` 循环快 10×（1M 次）。
- [ ] `strings.encode("你好")` → UTF-8 bytes。
- [ ] `strings.decode(bytes([72,105]))` → `"Hi"`。
- [ ] `strings.reverseStr("Hello")` → `"olleH"`。
- [ ] `strings.reverseStr("A😀B")` → `"B😀A"`（Unicode 码点级反转）。

---

## 测试用例（.ms）

```ms
import strings

// replace
print(strings.replace("foo foo foo", "foo", "bar", 2))  // bar bar foo

// Builder
sb := strings.Builder()
for i in range(5) { sb.write(str(i)); sb.write(",") }
print(sb.toString())  // 0,1,2,3,4,

// Unicode 安全反转
print(strings.reverseStr("Hello, 世界!"))  // !界世 ,olleH

// encode/decode
b := strings.encode("mslang")
print(b)               // b'mslang'
print(strings.decode(b))  // mslang
```

---

## Benchmark

```ms
// Builder vs 字符串 + 拼接
import strings, time
n := 100_000

// 慢（n 次 + 操作，O(n²)）
t0 := time.now()
s := ""
for i in range(n) { s = s + str(i) }
t1 := time.now()
print("concat:", t1-t0, "ms")

// 快（Builder，O(n)）
t0 = time.now()
sb := strings.Builder()
for i in range(n) { sb.write(str(i)) }
r := sb.toString()
t1 = time.now()
print("builder:", t1-t0, "ms")
// 目标：builder < concat × 10%
```
