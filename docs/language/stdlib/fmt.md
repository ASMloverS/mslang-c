# fmt — 格式化输出与字符串构建

```ms
import fmt
```

## 概述

Go 风格的格式化输出模块，提供 `printf`/`sprintf`/`fprintf`/`errorf` 等函数，补充内置 `print`
的不足（文件输出、格式字符串、结果构建）。格式占位符与 C `printf` 兼容，通用占位符 `%v`
调用对象的 `__repr__` 方法。建议项目统一使用 `print` 或 `fmt`，避免混用。

## 常量与类型

（无导出常量）

## 函数签名速查

| 函数/方法 | 签名 | 说明 |
|---|---|---|
| `fmt.println` | `println(*args)` | 空格分隔参数，末尾换行，写 stdout |
| `fmt.printf` | `printf(fmtStr, *args)` | 格式化输出到 stdout |
| `fmt.sprintf` | `sprintf(fmtStr, *args) → str` | 返回格式化字符串（不输出） |
| `fmt.fprintf` | `fprintf(file, fmtStr, *args)` | 格式化输出到 file 对象 |
| `fmt.fprintln` | `fprintln(file, *args)` | 空格分隔，末尾换行，写 file 对象 |
| `fmt.errorf` | `errorf(fmtStr, *args) → RuntimeError` | 构造 RuntimeError（仅返回，不 raise） |

## 格式占位符

| 占位符 | 类型 | 说明 |
|---|---|---|
| `%d` / `%i` | int | 十进制整数 |
| `%f` | float | 十进制浮点 |
| `%s` | str | 字符串（调用 `__str__`） |
| `%v` | any | 通用（调用 `__repr__`） |
| `%p` | any | 对象指针/地址（十六进制） |
| `%q` | str | 带双引号的转义字符串 |
| `%%` | — | 字面 `%` |

### 宽度与精度修饰符

| 修饰形式 | 示例 | 说明 |
|---|---|---|
| `%Nd` | `%8d` | 最小宽度 N，右对齐，不足补空格 |
| `%-Nd` | `%-8d` | 最小宽度 N，左对齐 |
| `%0Nd` | `%08d` | 最小宽度 N，右对齐，不足补零 |
| `%.Nf` | `%.2f` | 保留 N 位小数 |
| `%N.Mf` | `%10.2f` | 宽度 N，保留 M 位小数 |

## 详细语义

### println

```
fmt.println(*args)
```

将所有参数转换为字符串（调用 `__str__`），以单个空格分隔后写入 stdout，末尾追加换行符。
等价于 `print(*args, sep=" ", end="\n")`。无返回值。

### printf

```
fmt.printf(fmtStr, *args)
```

将 `fmtStr` 按占位符顺序依次替换 `args` 中的值，结果写入 stdout。不自动追加换行符。
`args` 数量必须与占位符数量一致，否则抛 `TypeError`。无返回值。

### sprintf

```
fmt.sprintf(fmtStr, *args) → str
```

与 `printf` 行为相同，但返回格式化后的字符串，不写入任何输出流。

### fprintf

```
fmt.fprintf(file, fmtStr, *args)
```

将格式化结果写入 `file` 对象。`file` 须实现 `write(s: str)` 方法（或兼容协议）。
写入失败时抛 `IOError`。无返回值。

### fprintln

```
fmt.fprintln(file, *args)
```

将所有参数以空格分隔后追加换行，写入 `file` 对象。`file` 须实现 `write(s: str)` 方法。
写入失败时抛 `IOError`。无返回值。

### errorf

```
fmt.errorf(fmtStr, *args) → RuntimeError
```

使用 `sprintf` 格式化消息，返回一个 `RuntimeError` 实例。**不抛出异常**，仅构造并返回。
调用方负责决定是否 `raise`。

```ms
err := fmt.errorf("invalid index: %d", idx)
if idx < 0 {
    raise err
}
```

## 示例

```ms
import fmt
import os

// 基本格式化输出
fmt.printf("name=%-10s age=%3d\n", "alice", 30)
// name=alice      age= 30

// 构建字符串（不输出）
msg := fmt.sprintf("%.2f%%", 98.6)
fmt.println(msg)  // 98.60%

// 写入文件
f := open("out.txt", "w")
fmt.fprintf(f, "%08d\n", 42)   // 00000042
fmt.fprintln(f, "done", "ok")  // done ok
f.close()

// 构造错误对象后按需抛出
func divide(a, b int) int {
    if b == 0 {
        raise fmt.errorf("divide by zero: %d / %d", a, b)
    }
    return a / b
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `TypeError` | 占位符类型与参数类型不匹配，或参数数量与占位符数量不一致 |
| `IOError` | `fprintf` / `fprintln` 向 file 对象写入失败 |
