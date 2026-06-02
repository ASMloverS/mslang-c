# gzip — gzip 压缩与解压

```ms
import gzip
```

## 概述

读写 gzip 格式（`.gz`）压缩文件，兼容 GNU gzip 工具链。参考 Python gzip 模块设计。

提供两类接口：

- **内存接口**（`compress`/`decompress`）：直接操作 `bytes` 对象，适合小数据块。
- **文件接口**（`gzip.open`）：返回文件对象，支持流式读写与 `with` 语句，
  适合大文件，避免一次性载入全部数据。

## 常量与类型

本模块不导出常量。

**GzipFile 对象**（由 `gzip.open` 返回）

| 属性 | 类型 | 说明 |
|---|---|---|
| `f.name` | `str` | 底层文件路径 |
| `f.mode` | `str` | 打开模式字符串 |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `open` | `open(filename, mode="rb", compresslevel=9, encoding=nil, errors=nil) → GzipFile` | 打开 gzip 文件 |
| `compress` | `compress(data, compresslevel=9, mtime=nil) → bytes` | 在内存中压缩字节串 |
| `decompress` | `decompress(data) → bytes` | 在内存中解压字节串 |

**GzipFile 对象方法**（实现文件接口）

| 方法 | 签名 | 说明 |
|---|---|---|
| `read` | `f.read(size=-1) → bytes\|str` | 读取最多 `size` 字节；`-1` 读全部 |
| `readline` | `f.readline() → bytes\|str` | 读取一行 |
| `readlines` | `f.readlines() → list` | 读取所有行为列表 |
| `write` | `f.write(data) → int` | 写入数据，返回写入字节数 |
| `close` | `f.close()` | 刷新并关闭文件 |

## 详细语义

### gzip.open

```
gzip.open(filename, mode="rb", compresslevel=9, encoding=nil, errors=nil) → GzipFile
```

打开或创建一个 gzip 文件，返回 GzipFile 对象。

**mode 取值：**

| 模式 | 说明 |
|---|---|
| `"rb"` | 二进制读（默认） |
| `"wb"` | 二进制写（创建或截断） |
| `"ab"` | 二进制追加 |
| `"rt"` | 文本读（解压后按行读取） |
| `"wt"` | 文本写（压缩前自动编码） |

文本模式（`"rt"`/`"wt"`）时，`encoding` 和 `errors` 参数传递给底层文本解码器，
默认使用 `"utf-8"` 编码。

`compresslevel`：压缩级别，`1`（最快，压缩率最低）到 `9`（最慢，压缩率最高），默认 `9`。
读模式下忽略此参数。

返回的 GzipFile 支持 `with` 语句：退出时自动调用 `close()`。

---

### gzip.compress

```
gzip.compress(data, compresslevel=9, mtime=nil) → bytes
```

将 `data`（`bytes`）压缩为 gzip 格式，返回压缩后的字节串。

- `mtime`：可选，覆盖 gzip 头部中的修改时间戳（Unix 时间戳整数）。
  传入 `0` 可生成确定性输出（相同输入总产生相同字节序列）。
  `nil` 时使用当前时间，导致输出不具有确定性。

适合小数据块的一次性压缩，对大数据请用 `gzip.open`。

---

### gzip.decompress

```
gzip.decompress(data) → bytes
```

解压 gzip 格式的字节串 `data`，返回原始字节。
`data` 包含多个拼接的 gzip 成员时，全部成员依次解压并拼接返回。

## 示例

```ms
import gzip

// 1. 在内存中压缩字符串
original := bytes("Hello, mslang! " * 100)
compressed := gzip.compress(original)
fmt.printf("原始: %d 字节, 压缩后: %d 字节\n", len(original), len(compressed))

// 2. 解压
restored := gzip.decompress(compressed)
fmt.println(restored == original)  // true

// 3. 写入 gzip 文件
with gzip.open("data.gz", "wb") as f {
    f.write(bytes("第一行内容\n"))
    f.write(bytes("第二行内容\n"))
}

// 4. 读取 gzip 文件（二进制模式）
with gzip.open("data.gz", "rb") as f {
    content := f.read()
    fmt.println(string(content))
}

// 5. 文本模式读写（自动处理编码）
with gzip.open("log.gz", "wt", encoding="utf-8") as f {
    f.write("2026-06-03 INFO 服务启动\n")
    f.write("2026-06-03 INFO 监听端口 8080\n")
}

with gzip.open("log.gz", "rt", encoding="utf-8") as f {
    for line := range f.readlines() {
        fmt.print(line)
    }
}

// 6. 确定性压缩（mtime=0，输出字节固定）
det := gzip.compress(bytes("fixed input"), mtime=0)
fmt.println(gzip.decompress(det))  // "fixed input"
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `gzip.BadGzipFile` | 文件不是合法 gzip 格式，或数据已损坏（`decompress`/读模式均可能触发） |
| `OSError` | 文件不存在、权限不足或其他 I/O 错误 |
| `TypeError` | `compress`/`decompress` 传入非 `bytes` 类型数据 |
