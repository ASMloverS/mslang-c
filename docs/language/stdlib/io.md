# io — I/O 抽象层

```ms
import io
```

## 概述

提供文件 I/O 对象、流抽象、异步文件读写以及标准输入输出流。
普通文件对象通过内置函数 `open()` 创建，`io` 模块主要提供：

- 标准流引用（`io.stdin` / `io.stdout` / `io.stderr`）
- 内存流（`io.BytesIO` / `io.StringIO`）
- 流工具函数（`io.copy`、`io.pipe`）
- 异步文件读写函数（`io.readFile` 等）

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `io.SEEK_SET` | `int = 0` | 从文件开头计算偏移 |
| `io.SEEK_CUR` | `int = 1` | 从当前位置计算偏移 |
| `io.SEEK_END` | `int = 2` | 从文件末尾计算偏移 |
| `io.stdin` | `File` | 标准输入流 |
| `io.stdout` | `File` | 标准输出流 |
| `io.stderr` | `File` | 标准错误流 |

**File 对象**（由 `open()` 或内存流构造函数返回）

| 成员 | 类型 | 说明 |
|---|---|---|
| `.name` | `str` | 文件路径或名称 |
| `.mode` | `str` | 打开模式字符串 |
| `.closed` | `bool` | 是否已关闭 |

## 函数签名速查

**文件对象方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `read` | `f.read(size=-1) → str\|bytes` | 读取至多 `size` 字节/字符；`-1` 读全部 |
| `readLine` | `f.readLine() → str\|bytes` | 读取一行（含换行符） |
| `readLines` | `f.readLines() → list` | 读取全部行，返回列表 |
| `write` | `f.write(data) → int` | 写入数据，返回写入字节/字符数 |
| `writeLines` | `f.writeLines(lines)` | 写入字符串序列（不自动添加换行） |
| `seek` | `f.seek(offset, whence=0)` | 移动文件指针 |
| `tell` | `f.tell() → int` | 返回当前文件指针位置 |
| `flush` | `f.flush()` | 刷新写缓冲区 |
| `close` | `f.close()` | 关闭文件 |
| `readable` | `f.readable() → bool` | 是否可读 |
| `writable` | `f.writable() → bool` | 是否可写 |
| `seekable` | `f.seekable() → bool` | 是否支持随机访问 |

**模块级函数**

| 函数 | 签名 | 说明 |
|---|---|---|
| `copy` | `io.copy(dst, src, bufSize=16384) → int` | 从 `src` 复制到 `dst`，返回字节数 |
| `pipe` | `io.pipe() → (reader, writer)` | 创建连接的读写流对 |
| `readFile` | `io.readFile(path) → str` | 异步读取整个文本文件（需 `await`） |
| `readFileBytes` | `io.readFileBytes(path) → bytes` | 异步读取整个二进制文件（需 `await`） |
| `writeFile` | `io.writeFile(path, data)` | 异步写入文本文件（需 `await`） |
| `writeFileBytes` | `io.writeFileBytes(path, data)` | 异步写入二进制文件（需 `await`） |

**内存流**

| 类 | 签名 | 说明 |
|---|---|---|
| `BytesIO` | `io.BytesIO(initial=nil)` | 基于内存的二进制流 |
| `StringIO` | `io.StringIO(initial=nil)` | 基于内存的文本流 |

## 详细语义

### open() — 打开文件

```
open(path, mode="r", encoding=nil, errors=nil, newline=nil) → File
```

内置函数（非 `io` 模块成员），返回 File 对象。

**模式字符串：**

| 模式 | 含义 |
|---|---|
| `"r"` | 只读文本（默认） |
| `"w"` | 只写文本，截断 |
| `"a"` | 追加文本 |
| `"x"` | 独占创建文本 |
| `"r+"` | 读写文本 |
| `"rb"` | 只读二进制 |
| `"wb"` | 只写二进制 |
| `"ab"` | 追加二进制 |
| `"rb+"` | 读写二进制 |

文本模式下 `encoding` 默认为 `"utf-8"`；
二进制模式下忽略 `encoding`，读写均返回/接受 `bytes`。

---

### f.read

```
f.read(size=-1) → str|bytes
```

从当前位置读取至多 `size` 字节（二进制）或字符（文本）。
`size=-1` 时读取到文件末尾。返回空字符串/空字节串表示到达 EOF。

---

### f.seek

```
f.seek(offset, whence=0)
```

移动文件读写指针。`whence` 取值：
- `0`（`io.SEEK_SET`）：从文件开头偏移 `offset` 字节
- `1`（`io.SEEK_CUR`）：从当前位置偏移 `offset` 字节
- `2`（`io.SEEK_END`）：从文件末尾偏移 `offset` 字节（`offset` 通常 ≤ 0）

文本模式下仅支持 `whence=0`（从开头），且 `offset` 必须来自 `f.tell()` 的返回值。

---

### with 语句与迭代

File 对象支持 `with` 语句：代码块退出时自动调用 `f.close()`，即使发生异常也会关闭。

文本模式 File 对象支持 `for line in f` 迭代，逐行读取，每行含末尾换行符。

```ms
with open("data.txt") as f {
    for line in f {
        fmt.print(line)
    }
}
```

---

### io.BytesIO / io.StringIO

```
io.BytesIO(initial=nil) → File
io.StringIO(initial=nil) → File
```

基于内存的流对象，实现全部 File 方法。`initial` 为初始内容（`bytes` 或 `str`）。
`.getValue()` 返回当前完整内容，不受指针位置影响。
适用于单元测试、构建中间数据、替代临时文件。

---

### io.copy

```
io.copy(dst, src, bufSize=16384) → int
```

分块将 `src` 流的内容复制到 `dst` 流，直至 `src` 到达 EOF。
返回实际复制的字节数。两个流均需处于正确的读/写模式。

---

### io.pipe

```
io.pipe() → (reader, writer)
```

创建一对连接的流对象：写入 `writer` 的数据可从 `reader` 读出。
适合在协程/goroutine 间传递数据，或将一个操作的输出直接接入另一个操作。

---

### 异步 I/O

```
io.readFile(path) → str
io.readFileBytes(path) → bytes
io.writeFile(path, data)
io.writeFileBytes(path, data)
```

所有异步函数均需配合 `await` 在 `async func` 内调用。
底层使用非阻塞 I/O，不会阻塞 goroutine 调度器。

```ms
async func loadConfig(path) {
    content := await io.readFile(path)
    return content
}
```

## 示例

```ms
import io

// 1. 读取文本文件
with open("README.md") as f {
    text := f.read()
    fmt.println($"文件大小：{len(text)} 字符")
}

// 2. 写入文件，逐行
lines := ["line1\n", "line2\n", "line3\n"]
with open("output.txt", "w") as f {
    f.writeLines(lines)
}

// 3. BytesIO 作为内存缓冲区
buf := io.BytesIO()
buf.write(bytes("header"))
buf.write(bytes(" body"))
buf.seek(0)
fmt.println(buf.read())  // b"header body"

// 4. StringIO 用于测试
fakeFile := io.StringIO("line one\nline two\n")
for line in fakeFile {
    fmt.print(line)
}

// 5. 异步读取
async func main() {
    data := await io.readFile("config.json")
    fmt.println(data)
}

// 6. io.copy 在两个流之间复制
with open("src.bin", "rb") as src {
    with open("dst.bin", "wb") as dst {
        n := io.copy(dst, src)
        fmt.println($"复制了 {n} 字节")
    }
}

// 7. seek / tell
with open("data.bin", "rb") as f {
    f.seek(0, io.SEEK_END)
    size := f.tell()
    fmt.println($"文件大小 = {size} 字节")
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `FileNotFoundError` | `open()` 或异步读函数路径不存在 |
| `PermissionError` | 无权限读写文件 |
| `IsADirectoryError` | 路径指向目录而非文件 |
| `OSError` | 底层 I/O 错误（磁盘满、断开连接等） |
| `ValueError` | 对已关闭的文件进行操作；`seek` 的 `whence` 非法；文本模式下使用非零 `whence` |
| `UnsupportedOperation` | 对不支持该操作的流调用（如对只读流调用 `write`） |
