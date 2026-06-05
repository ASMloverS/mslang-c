# tarfile — TAR 归档读写

```ms
import tarfile
```

## 概述

读写 tar 格式归档（`.tar`、`.tar.gz`/`.tgz`、`.tar.bz2`、`.tar.xz`），
支持文件与目录的递归归档、流式读写以及多种压缩格式。参考 Python tarfile 模块设计。

主入口为 `tarfile.open`，根据 `mode` 参数自动处理压缩/解压透明层。
归档操作完成后务必调用 `close()`，或使用 `with` 语句自动关闭。

**安全提示：** 解压不可信来源的归档时，请始终传入 `filter="data"` 以防止
路径穿越（path traversal）等目录攻击。

## 常量与类型

**TarInfo 对象属性：**

| 属性 | 类型 | 说明 |
|---|---|---|
| `ti.name` | `str` | 成员路径（归档内相对路径） |
| `ti.size` | `int` | 未压缩字节数（目录为 0） |
| `ti.mtime` | `int` | 修改时间（Unix 时间戳） |
| `ti.mode` | `int` | Unix 权限位（八进制，如 `0o644`） |
| `ti.type` | `bytes` | 成员类型（`tarfile.REGTYPE`/`DIRTYPE`/`SYMTYPE` 等） |
| `ti.linkname` | `str` | 符号链接/硬链接目标路径 |
| `ti.uid` | `int` | 属主用户 ID |
| `ti.gid` | `int` | 属主组 ID |
| `ti.uname` | `str` | 属主用户名 |
| `ti.gname` | `str` | 属主组名 |

**成员类型常量：**

| 常量 | 说明 |
|---|---|
| `tarfile.REGTYPE` | 普通文件 |
| `tarfile.DIRTYPE` | 目录 |
| `tarfile.SYMTYPE` | 符号链接 |
| `tarfile.LNKTYPE` | 硬链接 |

## 函数签名速查

| 函数/方法 | 签名 | 说明 |
|---|---|---|
| `open` | `open(name=nil, mode="r", fileobj=nil, bufsize=10240) → TarFile` | 打开归档 |
| `isTarfile` | `isTarfile(name) → bool` | 检测文件是否为有效 tar 归档 |
| `tf.getnames` | `tf.getnames() → list[str]` | 返回所有成员名称列表 |
| `tf.getmembers` | `tf.getmembers() → list[TarInfo]` | 返回所有成员 TarInfo 列表 |
| `tf.getmember` | `tf.getmember(name) → TarInfo` | 获取单个成员信息 |
| `tf.extractall` | `tf.extractall(path=".", members=nil, filter=nil)` | 解压全部或指定成员 |
| `tf.extract` | `tf.extract(member, path=".")` | 解压单个成员 |
| `tf.extractfile` | `tf.extractfile(member) → file \| nil` | 以文件对象方式读取成员 |
| `tf.add` | `tf.add(name, arcname=nil, recursive=true)` | 添加文件或目录 |
| `tf.addfile` | `tf.addfile(tarinfo, fileobj=nil)` | 从 TarInfo + 数据流添加成员 |
| `tf.close` | `tf.close()` | 刷新并关闭归档 |

## 详细语义

### tarfile.open

```
tarfile.open(name=nil, mode="r", fileobj=nil, bufsize=10240) → TarFile
```

打开或创建 tar 归档，返回 TarFile 对象。支持 `with` 语句。

**mode 取值：**

| 模式 | 说明 |
|---|---|
| `"r"` 或 `"r:*"` | 透明读取（自动检测压缩格式） |
| `"r:"` | 读取未压缩 tar |
| `"r:gz"` | 读取 gzip 压缩 tar |
| `"r:bz2"` | 读取 bzip2 压缩 tar |
| `"r:xz"` | 读取 xz/LZMA 压缩 tar |
| `"w"` 或 `"w:"` | 写入未压缩 tar（创建新文件） |
| `"w:gz"` | 写入 gzip 压缩 tar |
| `"w:bz2"` | 写入 bzip2 压缩 tar |
| `"w:xz"` | 写入 xz/LZMA 压缩 tar |
| `"a"` 或 `"a:"` | 追加到已有未压缩 tar（压缩格式不支持追加） |

- `name`：文件路径字符串；使用 `fileobj` 时可为 `nil`。
- `fileobj`：实现文件接口的对象，用于从内存或网络流读写归档。
- `bufsize`：内部读写缓冲区大小（字节），影响 I/O 性能，不影响功能。

---

### tarfile.isTarfile

```
tarfile.isTarfile(name) → bool
```

检测 `name` 指定的文件是否为有效 tar 归档（支持压缩格式自动检测）。
文件不存在或无法读取时返回 `false`（不抛异常）。

---

### tf.getmember

```
tf.getmember(name) → TarInfo
```

按名称查找成员，区分大小写。若归档中存在同名多个成员，返回最后一个。
成员不存在时抛 `KeyError`。

---

### tf.extractall

```
tf.extractall(path=".", members=nil, filter=nil)
```

将归档中的所有（或 `members` 列表中指定的）成员解压到 `path` 目录。

**filter 参数（安全关键）：**

| filter 值 | 说明 |
|---|---|
| `nil` | 无过滤（完全信任归档，仅用于受信任来源） |
| `"data"` | 安全过滤：拒绝绝对路径、`../` 路径穿越、设备文件、setuid 位等危险属性 |
| `"fully_trusted"` | 等同于 `nil`，语义上表示有意信任归档内容 |

**强烈建议**：解压来自网络、用户上传或第三方的归档时，始终指定 `filter="data"`。
未指定 `filter` 时，运行时将输出安全警告。

---

### tf.extract

```
tf.extract(member, path=".")
```

解压单个成员 `member`（`str` 路径名或 `TarInfo` 对象）到 `path` 目录。
不应用 `filter`，适合已经手动验证的成员路径。

---

### tf.extractfile

```
tf.extractfile(member) → file | nil
```

以只读文件对象形式打开归档成员，不将其解压到磁盘。
`member` 为目录或链接类型时返回 `nil`。
返回的文件对象支持 `read()`、`readline()`、`readlines()` 和 `with` 语句。

---

### tf.add

```
tf.add(name, arcname=nil, recursive=true)
```

将磁盘上的文件或目录 `name` 添加到归档。

- `arcname`：归档内存储路径；`nil` 时与 `name` 相同（不含驱动器盘符和前导分隔符）。
- `recursive=true`：`name` 为目录时递归添加所有子文件和子目录。
  `recursive=false` 时仅添加目录项本身。

---

### tf.addfile

```
tf.addfile(tarinfo, fileobj=nil)
```

使用预先构造的 `TarInfo` 对象添加成员，允许精确控制元数据（时间戳、权限、owner 等）。
若成员是普通文件，`fileobj` 提供数据内容（实现 `read()` 的文件对象）。
`fileobj=nil` 用于添加目录或链接等无数据内容的成员。

## 示例

```ms
import tarfile

// 1. 创建 .tar.gz 归档（递归打包目录）
with tarfile.open("project.tar.gz", "w:gz") as tf {
    tf.add("src/", arcname="src")
    tf.add("README.md")
    tf.add("LICENSE")
}

// 2. 列出归档内容
with tarfile.open("project.tar.gz", "r:gz") as tf {
    for info := range tf.getmembers() {
        fmt.printf("%-40s  %8d 字节  mode=%o\n",
            info.name, info.size, info.mode)
    }
}

// 3. 安全解压（来自不可信来源时必须使用 filter="data"）
with tarfile.open("downloaded.tar.gz", "r:gz") as tf {
    tf.extractall(path="./sandbox/", filter="data")
}

// 4. 流式读取单个成员（不解压到磁盘）
with tarfile.open("project.tar.gz", "r:gz") as tf {
    f := tf.extractfile("README.md")
    if f != nil {
        content := string(f.read())
        fmt.println(content)
    }
}

// 5. 透明模式（自动检测压缩格式）
with tarfile.open("unknown.tar.bz2", "r:*") as tf {
    names := tf.getnames()
    fmt.println("成员数:", len(names))
}

// 6. 追加到现有未压缩归档
with tarfile.open("archive.tar", "a") as tf {
    tf.add("new_file.ms")
}
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `tarfile.TarError` | 所有 tarfile 异常的基类 |
| `tarfile.ReadError` | 文件不是有效 tar 归档，或归档无法被正确读取 |
| `tarfile.CompressionError` | 不支持所请求的压缩方法，或压缩/解压过程中出错 |
| `tarfile.ExtractError` | 解压成员时出现非致命错误（如权限不足） |
| `tarfile.HeaderError` | tar 头部块损坏或无效 |
| `KeyError` | `getmember()` 找不到指定名称的成员 |
| `OSError` | 文件不存在、权限不足或其他 I/O 错误 |
