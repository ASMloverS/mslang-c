# re — 正则表达式（RE2 语法）

```ms
import re
```

## 概述

正则表达式模块，使用 RE2 语法：无回溯、无回顾（lookbehind）、无前瞻（lookahead），
保证线性时间匹配。替代旧版 `regexp` 模块，新代码应统一使用 `re`。编译后的 `Pattern`
对象可复用，避免重复解析开销。所有位置参数（`start`、`end`）均为字节偏移。

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `re.Pattern` | class | 编译后的正则模式对象（由 `re.compile` 返回） |
| `re.Match` | class | 匹配结果对象（由 `match`/`search`/`finditer` 返回） |

## 函数签名速查

### 模块级函数

| 函数 | 签名 | 说明 |
|---|---|---|
| `re.compile` | `compile(pattern) → Pattern` | 编译正则，返回 Pattern 对象 |
| `re.match` | `match(pattern, s) → Match \| nil` | 从字符串开头匹配 |
| `re.search` | `search(pattern, s) → Match \| nil` | 在字符串中任意位置搜索 |
| `re.findall` | `findall(pattern, s) → list[str]` | 返回所有非重叠匹配的字符串列表 |
| `re.finditer` | `finditer(pattern, s) → iterator[Match]` | 返回所有非重叠匹配的 Match 迭代器 |
| `re.sub` | `sub(pattern, repl, s, count=0) → str` | 替换匹配项；count=0 替换全部 |
| `re.split` | `split(pattern, s, maxsplit=0) → list[str]` | 按匹配项分割；maxsplit=0 不限次数 |
| `re.escape` | `escape(s) → str` | 转义字符串中所有正则特殊字符 |

### Pattern 方法

| 方法 | 签名 | 说明 |
|---|---|---|
| `p.match` | `p.match(s) → Match \| nil` | 从开头匹配 |
| `p.search` | `p.search(s) → Match \| nil` | 任意位置搜索 |
| `p.findall` | `p.findall(s) → list[str]` | 所有非重叠匹配字符串 |
| `p.finditer` | `p.finditer(s) → iterator[Match]` | 所有非重叠匹配的 Match 迭代器 |
| `p.sub` | `p.sub(repl, s, count=0) → str` | 替换匹配项 |
| `p.split` | `p.split(s, maxsplit=0) → list[str]` | 按匹配分割 |
| `p.groups` | `p.groups(s) → list[tuple]` | 返回 `[(full, g1, g2, ...), ...]` 列表 |

### Match 属性与方法

| 属性/方法 | 签名 | 说明 |
|---|---|---|
| `m.group` | `m.group(n=0) → str` | 第 n 组；0=整体匹配，1+ 为捕获组 |
| `m.groups` | `m.groups() → tuple[str]` | 所有捕获组的值（不含整体匹配） |
| `m.start` | `m.start(n=0) → int` | 第 n 组的起始字节偏移 |
| `m.end` | `m.end(n=0) → int` | 第 n 组的结束字节偏移（不含） |
| `m.span` | `m.span(n=0) → (int, int)` | `(m.start(n), m.end(n))` |
| `m.string` | `m.string` | 执行匹配的原始字符串 |

## 详细语义

### re.match vs re.search

```
re.match(pattern, s) → Match | nil
re.search(pattern, s) → Match | nil
```

`match` 要求模式从字符串**起始位置**开始匹配，等价于在 pattern 前隐式添加 `^`。
`search` 扫描整个字符串，返回第一个匹配位置的 `Match`。两者均在无匹配时返回 `nil`。

### re.findall

```
re.findall(pattern, s) → list[str]
```

若 pattern 含捕获组，返回各组内容的 tuple 列表（有多个组时）或字符串列表（仅一个组时）。
若无捕获组，返回整体匹配字符串的列表。

### re.sub

```
re.sub(pattern, repl, s, count=0) → str
```

`repl` 为字符串时，支持反向引用 `\1`、`\2` 等。`repl` 为函数时，每次匹配调用
`repl(match) → str`，返回值作为替换文本。`count=0` 替换全部匹配；`count=n` 最多替换 n 次。

### re.split

```
re.split(pattern, s, maxsplit=0) → list[str]
```

按 pattern 匹配处分割字符串。若 pattern 含捕获组，分组内容也包含在结果列表中（与
Python `re.split` 行为一致）。`maxsplit=0` 不限次数。

### re.escape

```
re.escape(s) → str
```

对字符串中所有具有正则含义的字符（`. ^ $ * + ? { } [ ] \ | ( )`）添加反斜杠转义，
用于将用户输入安全地嵌入正则表达式。

### Pattern.groups

```
p.groups(s) → list[tuple]
```

在 `s` 上执行 `finditer`，将每个 `Match` 转换为 `(full_match, group1, group2, ...)` tuple，
返回 tuple 列表。等价于 `[(m.group(0), *m.groups()) for m in p.finditer(s)]`。

### Match 对象

`m.group(0)` 返回整体匹配文本；`m.group(1)` 返回第一个捕获组，以此类推。
命名组（`(?P<name>...)`）可通过 `m.group("name")` 访问。
`m.start()`/`m.end()` 返回字节偏移（非码点偏移）。

## 示例

```ms
import re

// 基本搜索
m := re.search(r"\d+", "foo 42 bar")
if m != nil {
    fmt.println(m.group())   // 42
    fmt.println(m.start())   // 4
}

// 捕获组
p := re.compile(r"(\w+)@(\w+)\.(\w+)")
m2 := p.match("user@example.com")
if m2 != nil {
    fmt.println(m2.group(1))   // user
    fmt.println(m2.group(2))   // example
    fmt.println(m2.groups())   // ("user", "example", "com")
}

// findall
emails := re.findall(r"\b\w+@\w+\.\w+\b", "a@b.com and c@d.org")
fmt.println(emails)  // ["a@b.com", "c@d.org"]

// sub with string repl
result := re.sub(r"(\w+) (\w+)", r"\2 \1", "hello world")
fmt.println(result)  // world hello

// sub with function repl
result2 := re.sub(r"\d+", func(m) { return str(int(m.group()) * 2) }, "1 and 2 and 3")
fmt.println(result2)  // 2 and 4 and 6

// split
parts := re.split(r"\s*,\s*", "a, b,c ,  d")
fmt.println(parts)   // ["a", "b", "c", "d"]

// escape 用户输入
user_input := "3.14 (approx)"
safe := re.escape(user_input)
m3 := re.search(safe, "value is 3.14 (approx) here")
fmt.println(m3 != nil)  // true

// compile 复用
word_re := re.compile(r"\b\w{5}\b")
for line in lines {
    for w in word_re.findall(line) {
        fmt.println(w)
    }
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | 正则语法错误（如括号未匹配、RE2 不支持的语法）；或 `group(n)` 的 n 超出范围 |
| `TypeError` | `sub` 的 `repl` 既非字符串也非可调用对象；或参数类型错误 |
