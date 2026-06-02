# textwrap — 文本折行与缩进

```ms
import textwrap
```

## 概述

文本格式化工具，用于将长文本折行至指定列宽、调整缩进、截断文本。适用于 CLI 帮助文本、
日志美化、代码生成等场景。行为与 Python `textwrap` 标准库一致。折行以 Unicode 码点宽度
（而非字节数）计算列宽，全角字符计为宽度 2。

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `textwrap.TextWrapper` | class | 可配置的折行包装器 |

## 函数签名速查

| 函数/方法 | 签名 | 说明 |
|---|---|---|
| `wrap` | `wrap(text, width=70) → list[str]` | 折行，返回行列表（不含末尾换行） |
| `fill` | `fill(text, width=70) → str` | 折行后以 `\n` 连接为单个字符串 |
| `dedent` | `dedent(text) → str` | 去除所有行的公共前导空白 |
| `indent` | `indent(text, prefix, predicate=nil) → str` | 为每行（或满足条件的行）添加前缀 |
| `shorten` | `shorten(text, width, placeholder="...") → str` | 折叠空白后截断至 width，追加占位符 |

### TextWrapper 构造参数

| 参数 | 默认值 | 说明 |
|---|---|---|
| `width` | `70` | 最大列宽 |
| `initial_indent` | `""` | 首行前缀 |
| `subsequent_indent` | `""` | 后续行前缀 |
| `break_long_words` | `true` | 是否强制折断超长单词 |
| `break_on_hyphens` | `true` | 是否在连字符处折行 |

### TextWrapper 方法

| 方法 | 签名 | 说明 |
|---|---|---|
| `wrap` | `tw.wrap(text) → list[str]` | 同模块级 `wrap`，使用实例配置 |
| `fill` | `tw.fill(text) → str` | 同模块级 `fill`，使用实例配置 |

## 详细语义

### wrap

```
textwrap.wrap(text, width=70) → list[str]
```

将 `text` 中的连续空白折叠为单个空格，按 `width` 折行，返回字符串列表。列表中每个元素均
不含末尾换行符。若 `text` 为空字符串或纯空白，返回空列表 `[]`。`width` 须 ≥ 1，否则抛
`ValueError`。

### fill

```
textwrap.fill(text, width=70) → str
```

等价于 `strings.join("\n", textwrap.wrap(text, width))`。返回单个字符串，适合直接输出。

### dedent

```
textwrap.dedent(text) → str
```

计算所有**非空行**的公共前导空白（空格或制表符），从每行开头去除该前缀。常用于处理代码中
缩进的多行字符串字面量。空行不参与公共前缀计算，但在结果中保留。

### indent

```
textwrap.indent(text, prefix, predicate=nil) → str
```

为每行添加 `prefix`。若提供 `predicate`，则仅对 `predicate(line)` 返回真的行添加前缀；
`predicate` 接受包含末尾换行符的行字符串。默认（`predicate=nil`）对所有**非空行**（含仅有
空白的行）添加前缀，纯空行（`"\n"`）不添加前缀。

### shorten

```
textwrap.shorten(text, width, placeholder="...") → str
```

首先将 `text` 中所有空白折叠为单个空格并去除首尾空白，然后截断使结果（含 `placeholder`）
不超过 `width` 列宽。若原文本本身不超过 `width`，直接返回，不追加占位符。若 `width` 过小
以至于无法容纳任何单词加 `placeholder`，抛 `ValueError`。

### TextWrapper

```ms
tw := textwrap.TextWrapper(width=60, initial_indent="  ", subsequent_indent="    ")
lines := tw.wrap(long_text)
```

对同一配置需反复折行时，使用 `TextWrapper` 实例可避免重复传参。`break_long_words=false` 时，
超出 `width` 的单个单词不被强制折断，该行可能超出 `width`。

## 示例

```ms
import textwrap

// 基本折行
long := "The quick brown fox jumps over the lazy dog. " +
        "Pack my box with five dozen liquor jugs."
fmt.println(textwrap.fill(long, width=40))
// The quick brown fox jumps over the lazy
// dog. Pack my box with five dozen liquor
// jugs.

// dedent：去除多行字符串的公共缩进
code := "
    func hello() {
        print(\"hi\")
    }
"
fmt.println(textwrap.dedent(code))
// func hello() {
//     print("hi")
// }

// indent：为每行加前缀
block := "line one\nline two\nline three"
fmt.println(textwrap.indent(block, "> "))
// > line one
// > line two
// > line three

// 有条件的 indent：仅缩进非空行
mixed := "para one\n\npara two"
fmt.println(textwrap.indent(mixed, "  ", func(line) { return line.strip() != "" }))
// (空行不加前缀)

// shorten：截断长文本
fmt.println(textwrap.shorten("Hello world, this is a long sentence.", 20))
// Hello world, [...]
fmt.println(textwrap.shorten("Hello world, this is a long sentence.", 20, placeholder=" [more]"))
// Hello world, [more]

// TextWrapper：统一配置多次折行
tw := textwrap.TextWrapper(width=50, subsequent_indent="    ")
fmt.println(tw.fill("Usage: program [options] <input> <output>"))
// Usage: program [options] <input>
//     <output>
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `width` < 1；或 `shorten` 的 `width` 不足以容纳任何内容加占位符 |
| `TypeError` | `predicate` 不可调用；或参数类型错误 |
