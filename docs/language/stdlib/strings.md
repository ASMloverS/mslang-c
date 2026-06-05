# strings — 字符串处理

```ms
import strings
```

## 概述

Go 风格字符串处理模块，与 Python `str` 方法高度对应。所有函数均接受 string 并返回新 string，
遵循不可变语义。字符串方法同时作为 `str` 类型方法提供：`s.upper()` 等价于 `strings.upper(s)`。
包含 Unicode 辅助函数（无需单独 `import unicodedata`）。所有字节位置参数（`index`、`start`
等）均以字节偏移为单位；如需按码点计算，使用 `strings.codepointCount`。

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `strings.Builder` | class | 高效字符串拼接缓冲区 |

## 函数签名速查

| 函数/方法 | 签名 | 说明 |
|---|---|---|
| `contains` | `contains(s, substr) → bool` | 子串是否存在 |
| `hasPrefix` | `hasPrefix(s, prefix) → bool` | 是否以 prefix 开头 |
| `hasSuffix` | `hasSuffix(s, suffix) → bool` | 是否以 suffix 结尾 |
| `index` | `index(s, substr) → int` | 第一次出现的字节位置，未找到返回 -1 |
| `lastIndex` | `lastIndex(s, substr) → int` | 最后一次出现的字节位置，未找到返回 -1 |
| `count` | `count(s, substr) → int` | 不重叠出现次数 |
| `replace` | `replace(s, old, newS, n=-1) → str` | 替换前 n 次出现，n=-1 替换全部 |
| `split` | `split(s, sep, maxsplit=-1) → list` | 按 sep 分割，maxsplit=-1 不限次数 |
| `splitlines` | `splitlines(s) → list` | 按行分割（保留空行） |
| `join` | `join(sep, parts) → str` | 以 sep 连接 parts 中的字符串 |
| `strip` | `strip(s, chars=" \t\n\r") → str` | 去除两端字符 |
| `lstrip` | `lstrip(s, chars=" \t\n\r") → str` | 去除左端字符 |
| `rstrip` | `rstrip(s, chars=" \t\n\r") → str` | 去除右端字符 |
| `lower` | `lower(s) → str` | 转小写（Unicode） |
| `upper` | `upper(s) → str` | 转大写（Unicode） |
| `title` | `title(s) → str` | 每词首字母大写 |
| `repeat` | `repeat(s, n) → str` | 重复 n 次 |
| `trimPrefix` | `trimPrefix(s, prefix) → str` | 若有前缀则去除，否则返回原串 |
| `trimSuffix` | `trimSuffix(s, suffix) → str` | 若有后缀则去除，否则返回原串 |
| `fields` | `fields(s) → list` | 按空白分割，忽略连续空白与首尾空白 |
| `format` | `format(tmpl, *args, **kw) → str` | Python `str.format` 风格模板替换 |
| `isAlpha` | `isAlpha(s) → bool` | 所有码点是否均为字母（Unicode） |
| `isDigit` | `isDigit(s) → bool` | 所有码点是否均为数字（Unicode） |
| `isSpace` | `isSpace(s) → bool` | 所有码点是否均为空白（Unicode） |
| `codepointCount` | `codepointCount(s) → int` | Unicode 码点数（vs `len` 计字节数） |

### Builder 方法

| 方法 | 签名 | 说明 |
|---|---|---|
| `write` | `b.write(s: str)` | 追加字符串 |
| `writeByte` | `b.writeByte(n: int)` | 追加单个字节（0–255） |
| `string` | `b.string() → str` | 返回当前累积的字符串 |
| `len` | `b.len() → int` | 当前字节长度 |
| `reset` | `b.reset()` | 清空缓冲区 |

## 详细语义

### contains / hasPrefix / hasSuffix

```
strings.contains(s, substr) → bool
strings.hasPrefix(s, prefix) → bool
strings.hasSuffix(s, suffix) → bool
```

`contains` 等价于 `strings.index(s, substr) != -1`。空字符串始终匹配：
`strings.contains("abc", "")` 返回 `true`。

### index / lastIndex

```
strings.index(s, substr) → int
strings.lastIndex(s, substr) → int
```

返回子串首次/末次出现的**字节**偏移，未找到返回 `-1`。对多字节 UTF-8 字符，
偏移为该字符编码序列的首字节位置。

### replace

```
strings.replace(s, old, newS, n=-1) → str
```

从左至右替换 `old` 为 `newS`，最多替换 `n` 次。`n=-1` 替换全部。
`old=""` 时行为未定义，应避免传入空字符串作为 `old`。

### split / splitlines / fields

```
strings.split(s, sep, maxsplit=-1) → list
strings.splitlines(s) → list
strings.fields(s) → list
```

`split` 以 `sep` 为分隔符切割。`maxsplit` 指定最大切割次数（`-1` 无限制）。
若 `sep=""` 抛 `ValueError`。

`splitlines` 按 `\n`、`\r\n`、`\r` 切割，保留空行，末尾换行不产生额外空元素。

`fields` 以连续空白（空格、制表符、换行）为分隔符，首尾空白忽略，等价于
`strings.split(s.strip(), None)`（Python 语义）。

### join

```
strings.join(sep, parts) → str
```

注意参数顺序：分隔符在前，序列在后（Go 风格，与 Python `sep.join(parts)` 语义相同）。
`parts` 中元素须为 `str`，否则抛 `TypeError`。

### strip / lstrip / rstrip

```
strings.strip(s, chars=" \t\n\r") → str
strings.lstrip(s, chars=" \t\n\r") → str
strings.rstrip(s, chars=" \t\n\r") → str
```

`chars` 为字符集合（非子串），从对应端去除所有属于该集合的字符，直到遇到不属于集合的字符为止。

### trimPrefix / trimSuffix

```
strings.trimPrefix(s, prefix) → str
strings.trimSuffix(s, suffix) → str
```

若存在对应前/后缀则去除（仅去除一次），否则原样返回。不同于 `strip`，此处匹配的是完整子串而非字符集。

### format

```
strings.format(tmpl, *args, **kw) → str
```

Python `str.format` 风格：`{0}` 按位置，`{name}` 按关键字，`{}` 自动递增位置。
格式规范（如 `{:.2f}`）同样支持。

### isAlpha / isDigit / isSpace

```
strings.isAlpha(s) → bool
strings.isDigit(s) → bool
strings.isSpace(s) → bool
```

空字符串返回 `false`。基于 Unicode 分类判断，覆盖全部 Unicode 字母、数字和空白字符。

### codepointCount

```
strings.codepointCount(s) → int
```

返回字符串的 Unicode 码点数。对于纯 ASCII 字符串，结果与 `len(s)` 相同；
对于包含多字节字符的字符串，结果 ≤ `len(s)`。

### Builder

```ms
b := strings.Builder()
```

用于高效拼接大量字符串，避免 `+=` 的 O(n²) 问题。`write` 和 `writeByte` 均在 O(1)
均摊时间内完成。调用 `string()` 后，缓冲区数据仍然保留（不自动重置）。

## 示例

```ms
import strings

// 查找与判断
s := "Hello, 世界"
fmt.println(strings.contains(s, "世界"))       // true
fmt.println(strings.hasPrefix(s, "Hello"))   // true
fmt.println(strings.index(s, "世界"))          // 7（字节偏移）
fmt.println(strings.codepointCount(s))       // 9（码点数）
fmt.println(len(s))                            // 13（字节数）

// 分割与连接
words := strings.split("a,b,,c", ",")
fmt.println(words)               // ["a", "b", "", "c"]
fmt.println(strings.fields("  foo  bar  baz  "))  // ["foo", "bar", "baz"]
fmt.println(strings.join("-", words))             // a-b--c

// 替换与修整
fmt.println(strings.replace("aabbcc", "b", "X", 1))  // aaXbcc
fmt.println(strings.trimPrefix("foobar", "foo"))     // bar
fmt.println(strings.strip("  hello  "))               // hello

// 大小写
fmt.println(strings.upper("héllo"))  // HÉLLO
fmt.println(strings.title("the quick brown fox"))  // The Quick Brown Fox

// Builder 高效拼接
b := strings.Builder()
for i in range(5) {
    b.write($"{i} ")
}
fmt.println(b.string())  // 0 1 2 3 4

// format 模板
fmt.println(strings.format("{name} is {age}", name="alice", age=30))
// alice is 30
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `TypeError` | `join` 的 parts 包含非字符串元素；`writeByte` 传入超出 0–255 范围的值 |
| `ValueError` | `split` 传入空字符串 sep；`repeat` 传入负数 n |
