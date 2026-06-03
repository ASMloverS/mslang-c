# tempfile — 临时文件与目录

```ms
import tempfile
```

## 概述

提供创建临时文件和目录的接口，参考 Python tempfile 模块设计。
所有临时对象默认在不再需要时自动清理，避免手动删除的负担。

选择指南：
- 需要文件路径（如传给子进程）→ `NamedTemporaryFile`
- 只需要文件对象（内部使用）→ `TemporaryFile`
- 需要临时目录 → `TemporaryDirectory`
- 需要控制生命周期（手动清理）→ `mkstemp` / `mkdtemp`
- 小数据优先内存、超出后落盘 → `SpooledTemporaryFile`

## 常量与类型

`tempfile` 模块无独立常量。临时文件/目录的默认存储位置由 `tempfile.gettempdir()` 返回，
通常为系统临时目录（`/tmp`、`%TEMP%` 等）。

## 函数签名速查

**低层函数（手动管理生命周期）**

| 函数 | 签名 | 说明 |
|---|---|---|
| `mkstemp` | `tempfile.mkstemp(suffix=nil, prefix="tmp", dir=nil, text=false) → (int, str)` | 创建临时文件，返回 `(fd, path)` |
| `mkdtemp` | `tempfile.mkdtemp(suffix=nil, prefix="tmp", dir=nil) → str` | 创建临时目录，返回路径 |
| `gettempdir` | `tempfile.gettempdir() → str` | 返回系统临时目录路径 |
| `gettempprefix` | `tempfile.gettempprefix() → str` | 返回默认文件名前缀 |

**上层类（自动管理生命周期）**

| 类 | 说明 |
|---|---|
| `tempfile.NamedTemporaryFile(...)` | 有名临时文件（路径可见），支持 `with` |
| `tempfile.TemporaryFile(...)` | 无名（或平台相关）临时文件，支持 `with` |
| `tempfile.TemporaryDirectory(...)` | 临时目录，支持 `with` |
| `tempfile.SpooledTemporaryFile(...)` | 先驻内存、超限后落盘的临时文件 |

## 详细语义

### tempfile.mkstemp

```
tempfile.mkstemp(suffix=nil, prefix="tmp", dir=nil, text=false) → (int, str)
```

以原子方式创建一个唯一临时文件，返回 `(fd, path)` 元组：
- `fd`：操作系统级文件描述符（整数），调用者负责用 `os.close(fd)` 关闭。
- `path`：临时文件的完整路径字符串，调用者负责用 `os.remove(path)` 删除。

`mkstemp` 不会自动清理，适合需要精确控制生命周期的场景。

- `suffix`：文件名后缀（如 `".txt"`）；`nil` 时无后缀。
- `prefix`：文件名前缀，默认 `"tmp"`。
- `dir`：创建于此目录；`nil` 时使用 `gettempdir()`。
- `text=false`：以二进制模式创建；`text=true` 以文本模式打开。

---

### tempfile.mkdtemp

```
tempfile.mkdtemp(suffix=nil, prefix="tmp", dir=nil) → str
```

以原子方式创建一个唯一临时目录，返回其完整路径。
调用者负责在使用完毕后调用 `shutil.rmtree(path)` 删除目录及其内容。

---

### tempfile.NamedTemporaryFile

```
tempfile.NamedTemporaryFile(
    mode="w+b",
    suffix=nil,
    prefix="tmp",
    dir=nil,
    delete=true,
    encoding=nil,
) → File
```

创建一个有可见路径名的临时文件对象，实现全部 File 方法。

- `.name → str`：临时文件的完整路径，可传递给其他进程或函数。
- `delete=true`：文件关闭（或 `with` 块退出）时自动删除；`false` 则保留。
- `mode`：默认 `"w+b"`（二进制读写），可指定为文本模式 `"w+"` 等。
- `encoding`：文本模式下的编码；二进制模式忽略。

支持 `with` 语句：退出时调用 `close()`，若 `delete=true` 则同时删除文件。

**注意（Windows）：** Windows 默认不允许其他进程在文件打开时通过名称访问它。
若需将 `.name` 传给子进程，使用 `delete=false` 并手动清理，
或改用 `mkstemp`。

---

### tempfile.TemporaryFile

```
tempfile.TemporaryFile(
    mode="w+b",
    suffix=nil,
    prefix=nil,
    dir=nil,
    encoding=nil,
) → File
```

创建临时文件对象。与 `NamedTemporaryFile` 的区别：
- 在 POSIX 上，文件创建后立即从目录中取消链接（unlink），
  无可见路径，不会泄露文件名，安全性更高。
- 关闭时自动删除（无 `delete` 参数）。
- 无 `.name` 属性（或值无意义）。

适合只需要文件对象语义、不需要路径的场景（如临时序列化、管道缓冲）。

---

### tempfile.TemporaryDirectory

```
tempfile.TemporaryDirectory(
    suffix=nil,
    prefix="tmp",
    dir=nil,
    ignore_cleanup_errors=false,
) → TemporaryDirectory
```

创建临时目录对象。

- `.name → str`：临时目录路径，可在 `with` 块内使用。
- `with` 块退出时递归删除目录及其全部内容。
- `ignore_cleanup_errors=true`：清理时忽略错误（如文件被占用）。
- 也可调用 `.cleanup()` 方法手动提前清理。

---

### tempfile.SpooledTemporaryFile

```
tempfile.SpooledTemporaryFile(
    max_size=0,
    mode="w+b",
    suffix=nil,
    prefix=nil,
    dir=nil,
    encoding=nil,
) → File
```

混合内存/磁盘临时文件：数据量不超过 `max_size` 字节时驻留在 `io.BytesIO`/`io.StringIO`；
超出后自动将内容落盘到真实临时文件。

`max_size=0` 时始终驻留内存（永不落盘）。
实现全部 File 方法；支持 `with` 语句，退出时自动清理。

---

### tempfile.gettempdir / gettempprefix

```
tempfile.gettempdir() → str
tempfile.gettempprefix() → str
```

`gettempdir()` 按以下顺序搜索可写目录：
`TMPDIR` → `TEMP` → `TMP` 环境变量 → 平台默认目录（`/tmp` 或 `%TEMP%`）。

`gettempprefix()` 返回默认的临时文件名前缀字符串（通常为 `"tmp"`）。

## 示例

```ms
import tempfile
import os
import shutil

// 1. NamedTemporaryFile with 语句（最常用）
with tempfile.NamedTemporaryFile(suffix=".json", mode="w+", encoding="utf-8") as f {
    f.write('{"key": "value"}')
    f.seek(0)
    fmt.println(f.read())
    fmt.println($"临时文件路径：{f.name}")
    // with 块结束后文件自动删除
}

// 2. TemporaryDirectory with 语句
with tempfile.TemporaryDirectory(prefix="build_") as tmpdir {
    out_path := os.path.join(tmpdir, "output.bin")
    with open(out_path, "wb") as f {
        f.write(bytes("binary data"))
    }
    fmt.println($"临时目录：{tmpdir}")
    // with 块结束后目录及内容自动删除
}

// 3. mkstemp 手动管理
fd, path := tempfile.mkstemp(suffix=".tmp")
try {
    // 通过文件描述符写入
    os.write(fd, bytes("raw data"))
} finally {
    os.close(fd)
    os.remove(path)
}

// 4. mkdtemp 手动管理
tmpdir := tempfile.mkdtemp(prefix="cache_")
try {
    // 使用临时目录
    shutil.copy("data.bin", os.path.join(tmpdir, "data.bin"))
} finally {
    shutil.rmtree(tmpdir)
}

// 5. SpooledTemporaryFile：小数据内存，大数据落盘
with tempfile.SpooledTemporaryFile(max_size=1024*1024, mode="w+b") as f {
    f.write(bytes("small payload"))  // 驻内存
    f.seek(0)
    fmt.println(f.read())
}

// 6. 查询临时目录
fmt.println($"系统临时目录：{tempfile.gettempdir()}")
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `FileNotFoundError` | `dir` 参数指定的目录不存在 |
| `PermissionError` | 无权在目标目录创建文件/目录 |
| `OSError` | 磁盘满或其他底层 I/O 错误 |
| `ValueError` | 对已关闭的临时文件对象进行操作 |
