# mslang 脚本编码规范

> 版本 1.0 | 适用范围：`.ms` 脚本文件 | 最后更新：2026-06-04

## 目录

1. [引言](#1-引言)
2. [源文件结构](#2-源文件结构)
3. [import 语句](#3-import-语句)
4. [格式排版](#4-格式排版)
5. [命名约定](#5-命名约定)
6. [注释](#6-注释)
7. [编程惯例](#7-编程惯例)

---

## 1. 引言

本规范适用于所有 **mslang 脚本文件**（扩展名 `.ms`）。

格式与结构参考 [Google Java Style Guide](https://google.github.io/styleguide/javaguide.html)，
命名与注释风格与本项目 `c-style.md` 保持一致。

> **语法权威**：字面量形式、运算符、关键字、异常模型等语法细节以 `syntax.md` 和 `errors.md` 为准；
> 本规范仅约定风格，不重定义语法。

### 1.1 优先级

1. **本规范**优先于个人习惯。
2. **一致性优先**：在已有代码库中，优先与周边代码风格保持一致；新建文件时遵循本规范。
3. **可读性**：所有规则以「让陌生人在最短时间内读懂代码」为最终目标。

### 1.2 源文件基础

| 要求 | 说明 |
|------|------|
| 字符编码 | UTF-8，无 BOM |
| 换行符 | LF（`\n`），**禁止** CRLF |
| 行尾空白 | 必须清除，任何行不得以空格或 Tab 结尾 |
| 文件结尾 | 以单个换行符结尾 |

---

## 2. 源文件结构

### 2.1 文件命名

`.ms` 文件使用 **snake_case**（小写字母 + 下划线）命名。

```
// 正例
user_service.ms
http_client.ms
parse_utils.ms

// 反例
UserService.ms     // 禁止 PascalCase
httpClient.ms      // 禁止 camelCase
http-client.ms     // 禁止 kebab-case
```

### 2.2 顶层元素顺序

一个源文件中，各顶层元素按如下顺序排列：

1. 文件头注释（可选）
2. `import` 语句
3. 模块级常量
4. 类（`class`）定义
5. 函数（`func`）定义

**顶层元素之间空 2 行**。同一分组（如多个紧密相关的常量）内部可仅空 1 行。

### 2.3 文件头注释

文件开头可加 `//` 注释简述模块功能与版权（如有）：

```ms
// http_client.ms
// HTTP/1.1 client with connection pooling and retry support.
//
// Copyright (C) 2026 Example Corp. All rights reserved.
```

---

## 3. import 语句

### 3.1 分组与排序

`import` 语句按如下三组排列，**组与组之间留一个空行**，每组内部**按字母顺序**排列：

1. **内置模块**（mslang 标准库）
2. **第三方模块**
3. **本地模块**（相对路径，以 `.` 或 `..` 开头）

```ms
// 正例
import math
import os
import strings

import third_party.auth
import third_party.log

import .utils
import ..common.config
```

### 3.2 别名（as）

仅在以下情况使用别名，避免滥用缩写：

- 模块名与当前文件中已有的标识符冲突
- 第三方模块名过长且在文件中高频使用

```ms
// 正例：缩短高频使用的长名
import third_party.serialization as ser

// 反例：无意义的缩写
import strings as s
```

---

## 4. 格式排版

### 4.1 缩进

- 每级缩进 **4 个空格**，**禁止使用 Tab**。
- 同一文件不得混用空格与 Tab。

### 4.2 列宽

每行不超过 **120 个字符**。注释或字符串字面量超出时可酌情保留，但应尽量拆分。

### 4.3 大括号（K&R 风格）

开括号 `{` 跟在同行末尾；闭括号 `}` 独占一行。
`else`、`catch`、`finally` 紧跟 `}` 同行：

```ms
// 正例
if condition {
    doA()
} else if other {
    doB()
} else {
    doC()
}

func process(data) {
    // ...
}

try {
    riskyOp()
} catch (e) {
    handleErr(e)
} finally {
    cleanup()
}

// 反例
func process(data)
{                    // 禁止：左括号换行
    // ...
}

if condition {
    doA()
}
else {               // 禁止：else 单独起行
    doB()
}
```

所有控制流语句（`if`、`for`、`try`、`func`）的语句体**必须使用大括号**，即使语句体仅一行也不允许省略：

```ms
// 反例（语法错误）
if x > 0 return x
for v in list print(v)

// 正例
if x > 0 {
    return x
}
for v in list {
    print(v)
}
```

### 4.4 续行缩进

当语句超过列宽需要换行时，续行使用 **+8 个空格**（相对于该语句第一行的起始缩进，即双层缩进）：

```ms
// 函数声明参数换行
func doSomethingWithLongName(
        firstParam,
        secondParam,
        thirdParam) {
    body()
}

// 函数调用参数换行
result := someFunction(
        firstArg,
        secondArg,
        thirdArg)

// 二元表达式换行：在运算符前换行
isValid := isNotEmpty(name)
        && isValidEmail(email)
        && hasPermission(userId)
```

参数换行时，每个参数独占一行，不要同行混排：

```ms
// 正例
result := compute(
        alpha,
        beta,
        gamma)

// 反例
result := compute(alpha, beta,
        gamma)
```

### 4.5 空格规则

| 场景 | 规则 | 示例 |
|------|------|------|
| 二元运算符 | 两侧各一空格 | `a + b`、`x == y`、`i < 10` |
| 符号一元运算符 | 贴操作数，无空格 | `-x`、`~x`、`i++` |
| 关键字一元运算符 | 与操作数间留一空格 | `not flag`、`await f()` |
| 逗号 | 后加空格，前不加 | `foo(a, b, c)` |
| 关键字与 `(` | 加空格 | `catch (e)`、`make(chan, 16)` — 注：`if`/`for` 条件不加括号 |
| 函数名与 `(` | 不加空格 | `foo(x)` 而非 `foo (x)` |
| `{}` 字面量内 | 不加空格 | `{"a": 1}`、`{1, 2}` |
| `[]` 下标 | 不加空格 | `arr[i]` 而非 `arr[ i ]` |
| f-string `{}` | 不加空格 | `$"{name}"` 而非 `$"{ name }"` |

### 4.6 空行

- **顶层声明之间**（func、class、模块级常量组）：空 **2 行**。
- **类内方法之间**：空 **1 行**。
- **函数体内**：最多 1 个空行用于逻辑分段；禁止连续 2 个以上空行。
- **文件末尾**：恰好一个换行符。

```ms
// 正例：顶层间 2 行
MAX_SIZE := 1024


func processOne(item) {
    // ...
}


func processAll(items) {
    // ...
}


class Worker {

    func __init__(self, id) {
        self.id = id
    }

    func run(self) {
        // ...
    }

    func stop(self) {
        // ...
    }
}
```

### 4.7 switch 语句

`case` 标签与 `switch` **同级**（不额外缩进），`case` 体缩进 4 空格：

```ms
// 正例
switch status {
case 0:
    handleOk()
case 1, 2:
    handleWarn()
default:
    handleError()
}

// 反例
switch status {
    case 0:        // 禁止：case 额外缩进
        handleOk()
}
```

---

## 5. 命名约定

### 5.1 总览

| 标识符类型 | 风格 | 示例 |
|---|---|---|
| 函数 | camelCase | `getUserById`、`parseHeader` |
| 参数 | camelCase | `maxRetries`、`userId` |
| 局部变量 | camelCase | `totalCount`、`isValid` |
| 模块级常量 | UPPER\_SNAKE\_CASE | `MAX_SIZE`、`DEFAULT_TIMEOUT` |
| 类 | PascalCase | `UserAccount`、`HttpClient` |
| 异常类 | PascalCase | `NetworkError`、`ParseError` |
| 模块/文件 | snake\_case | `user_service`、`http_client` |
| 魔法方法 | `__dunder__` | `__init__`、`__len__`、`__str__` |

### 5.2 函数命名

函数名使用 **camelCase**，以动词或动词短语开头，清晰表达行为：

```ms
// 正例
func getUserById(userId) { ... }
func parseHttpHeader(raw) { ... }
func isValidEmail(email) { ... }
func sendAll(items) { ... }

// 反例
func GetUserById(userId) { ... }     // 禁止 PascalCase
func get_user_by_id(userId) { ... }  // 禁止 snake_case
func user(userId) { ... }            // 不表达行为
```

### 5.3 变量与参数命名

使用 **camelCase**，名称有意义，不使用单字母（循环变量 `i`/`j`/`k` 除外）：

```ms
// 正例
retryCount := 0
var userName
for i := 0; i < len(items); i++ { ... }

// 反例
rc := 0            // 过度缩写，含义不明
retry_count := 0   // 禁止 snake_case
```

### 5.4 模块级常量

模块顶层的不可变绑定（表示配置常量、魔法数字等）使用 **UPPER\_SNAKE\_CASE**：

```ms
// 正例
MAX_RETRIES := 3
DEFAULT_TIMEOUT := 30
BASE_URL := "https://api.example.com"

// 反例
maxRetries := 3    // 与普通变量无法区分
kMaxRetries := 3   // 本规范不采用 k 前缀
```

### 5.5 类与异常类命名

类名使用 **PascalCase**，包括异常类：

```ms
// 正例
class UserAccount { ... }
class HttpClient { ... }
class NetworkError extends Exception { ... }
class ParseError extends Exception { ... }

// 反例
class userAccount { ... }    // 禁止 camelCase
class http_client { ... }    // 禁止 snake_case
```

异常类名应以 `Error` 或 `Exception` 结尾，明确其可被 `raise`/`catch`。

### 5.6 魔法方法

遵循语言规定的 **`__dunder__`** 格式；不得自定义双下划线包裹的方法名，以免与语言保留名冲突：

```ms
// 正例（语言定义的魔法方法）
func __init__(self, ...) { ... }
func __len__(self) { ... }
func __str__(self) { ... }

// 反例：自定义双下划线方法
func __myHelper__(self) { ... }  // 禁止
```

---

## 6. 注释

### 6.1 只使用 `//` 注释

**禁止使用 `/* */`**（包括单行与多行形式）。统一使用 `//`，原因：

- 可用 `/* */` 临时整块注释含 `//` 的代码段，两者不冲突。
- 避免 `/* */` 嵌套失效问题。
- 风格统一，无需区分场景。

### 6.2 文档注释

公共 `func` 和 `class` 应在声明**前**加文档注释：

- **第一行**：一句话摘要，完整句子，句末加句号。
- **后续行**（可选）：参数说明、返回值、可能抛出的异常、线程安全性等。

文档注释与声明之间**不留空行**。

```ms
// Returns the user record matching the given ID.
// Returns nil if no such user exists.
//
// userId: the unique identifier to search for; must be > 0.
func getUserById(userId) {
    // ...
}

// Manages a pool of reusable HTTP connections.
//
// Not safe for concurrent use across parallel go tasks without external locking.
class ConnectionPool {
    // ...
}
```

### 6.3 行内注释

仅用于解释**非显而易见**的逻辑：隐藏的约束、反直觉的行为、绕过已知 bug 的手段。
注释说明**为什么**，而非重复代码在做什么：

```ms
// 正例
timeout *= 2  // exponential back-off; capped upstream by MAX_RETRIES

// 反例
i++  // increment i    （仅重复代码，无价值）
```

### 6.4 TODO 注释

格式：`// TODO(<owner>): <描述>`

```ms
// TODO(alice): replace linear scan with hash map when list > 1000 items
```

`owner` 可以是用户名或追踪系统的 issue 编号。

---

## 7. 编程惯例

### 7.1 字符串字面量

优先使用 **双引号** `"..."`：

```ms
// 正例
name := "Alice"
msg := "Hello, World!"

// 反例
name := 'Alice'  // 错误：ms 无单引号字符串（syntax.md §1.8 仅支持双引号）
```

字符串内含双引号时，转义即可：

```ms
msg := "He said \"hello\""  // 转义双引号
```

### 7.2 f-string（插值字符串）

使用 `$"..."` 语法，花括号内不加多余空格：

```ms
// 正例
greeting := $"Hello, {name}!"
info := $"User {userId} logged in at {timestamp}"

// 反例
info := $"User { userId } logged in"  // 禁止花括号内加空格
```

### 7.3 集合字面量

末尾逗号（trailing comma）在多行集合中**可选**，但同一文件内应保持一致：

```ms
// 单行集合：不加末尾逗号
items := [1, 2, 3]
opts := {"retry": 3, "timeout": 30}

// 多行集合：末尾逗号可选
matrix := [
    [1, 0, 0],
    [0, 1, 0],
    [0, 0, 1],   // 有末尾逗号
]

labels := [
    "alpha",
    "beta",
    "gamma"      // 无末尾逗号
]
```

### 7.4 变量声明

`:=` 简短声明和 `var` 显式声明均可使用，以下场景推荐 `var`：

- 声明后暂不赋值，等待后续条件分支赋值
- 需要在视觉上突出该变量的生命周期起点

```ms
// 两者均合法
x := 42
var x = 42

// var 适合：声明后稍后赋值
var result
if condition {
    result = computeA()
} else {
    result = computeB()
}
```
