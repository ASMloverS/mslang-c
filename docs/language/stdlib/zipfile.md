# zipfile — ZIP 归档读写

```ms
import zipfile
```

## 概述

读写 ZIP 格式归档文件，支持压缩（deflate/stored）、多种压缩算法、
密码保护读取以及流式写入。参考 Python zipfile 模块设计。

主入口为 `zipfile.ZipFile` 类，提供完整的归档管理功能。
`zipfile.isZipfile` 提供快速格式检测。

## 常量与类型

**压缩方法常量：**

| 常量 | 值 | 说明 |
|---|---|---|
| `zipfile.ZIP_STORED` | `0` | 不压缩，仅归档 |
| `zipfile.ZIP_DEFLATED` | `8` | zlib deflate 压缩（最常用） |
| `zipfile.ZIP_BZIP2` | `12` | bzip2 压缩 |
| `zipfile.ZIP_LZMA` | `14` | LZMA 压缩（最高压缩率） |

**ZipInfo 对象属性：**

| 属性 | 类型 | 说明 |
|---|---|---|
| `zi.filename` | `str` | 归档内成员路径 |
| `zi.dateTime` | `tuple` | `(year, month, day, hour, min, sec)` |
| `zi.compressType` | `int` | 压缩方法常量 |
| `zi.compressSize` | `int` | 压缩后字节数 |
| `zi.fileSize` | `int` | 原始字节数 |
| `zi.comment` | `bytes` | 成员注释 |

## 函数签名速查

| 函数/方法 | 签名 | 说明 |
|---|---|---|
| `isZipfile` | `isZipfile(filename) → bool` | 检测文件是否为有效 ZIP |
| `ZipFile` | `ZipFile(file, mode="r", compression=ZIP_DEFLATED, allowZip64=true)` | 打开/创建归档 |
| `zf.nameList` | `zf.nameList() → list[str]` | 返回所有成员名称列表 |
| `zf.infoList` | `zf.infoList() → list[ZipInfo]` | 返回所有成员 ZipInfo 列表 |
| `zf.getInfo` | `zf.getInfo(name) → ZipInfo` | 获取单个成员信息 |
| `zf.read` | `zf.read(name) → bytes` | 按名称读取成员内容 |
| `zf.open` | `zf.open(name, mode="r") → file` | 以文件对象方式访问成员 |
| `zf.write` | `zf.write(filename, arcName=nil, compressType=nil)` | 从磁盘添加文件 |
| `zf.writeStr` | `zf.writeStr(nameOrInfo, data)` | 直接写入数据为归档成员 |
| `zf.extract` | `zf.extract(member, path=nil)` | 解压单个成员 |
| `zf.extractAll` | `zf.extractAll(path=nil, members=nil)` | 解压全部或指定成员 |
| `zf.close` | `zf.close()` | 关闭并写入归档尾部 |

## 详细语义

### zipfile.isZipfile

```
zipfile.isZipfile(filename) → bool
```

检查文件是否以有效的 ZIP 魔数（`PK\x03\x04`）开头并包含有效目录结构。
文件不存在或无法读取时返回 `false`（不抛异常）。

---

### zipfile.ZipFile

```
zipfile.ZipFile(file, mode="r", compression=ZIP_DEFLATED, allowZip64=true)
```

打开或创建 ZIP 归档，返回 ZipFile 对象。支持 `with` 语句。

**mode 取值：**

| 模式 | 说明 |
|---|---|
| `"r"` | 只读，打开已有归档 |
| `"w"` | 写入，创建新归档（已有文件将被截断） |
| `"a"` | 追加，向已有归档添加成员 |
| `"x"` | 排他写入，文件已存在时抛 `FileExistsError` |

- `compression`：默认压缩方法，可被 `write`/`writeStr` 的 `compressType` 参数覆盖。
- `allowZip64=true`：允许生成超过 4 GB 的归档或含超过 65535 个成员的归档（ZIP64 扩展）。
  设为 `false` 时，超限则抛 `LargeZipFile`。

---

### zf.read

```
zf.read(name) → bytes
```

一次性读取成员 `name` 的全部内容并返回字节串。
成员不存在时抛 `KeyError`。

---

### zf.open

```
zf.open(name, mode="r") → file
```

以文件对象形式打开成员，适合流式读取大成员。返回的对象支持 `with` 语句。
`mode` 目前仅支持 `"r"`。

---

### zf.write

```
zf.write(filename, arcName=nil, compressType=nil)
```

将磁盘文件 `filename` 添加到归档。

- `arcName`：归档内存储的路径名；`nil` 时使用 `filename` 并去掉根路径前缀。
- `compressType`：覆盖该成员的压缩方法；`nil` 时使用 ZipFile 构造时的 `compression`。

---

### zf.writeStr

```
zf.writeStr(nameOrInfo, data)
```

将 `data`（`bytes` 或 `str`）直接写入归档，无需从磁盘读取文件。
`nameOrInfo` 可以是成员路径字符串，也可以是预先创建的 `ZipInfo` 对象
（用于精确控制元数据如修改时间、权限等）。

`str` 类型的 `data` 以 UTF-8 编码。

---

### zf.extract / zf.extractAll

```
zf.extract(member, path=nil)
zf.extractAll(path=nil, members=nil)
```

- `path`：解压目标目录；`nil` 时解压到当前工作目录。
- `members`：`extractAll` 的可选成员名列表；`nil` 时解压所有成员。

注意：解压不可信来源的归档时，成员路径中可能含有 `../` 等路径穿越序列。
应在解压前手动校验 `nameList()` 中每个路径，或使用受信任来源的归档。

## 示例

```ms
import zipfile

// 1. 创建 ZIP 归档并添加文件
with zipfile.ZipFile("archive.zip", "w", zipfile.ZIP_DEFLATED) as zf {
    zf.write("README.md")
    zf.write("src/main.ms", arcName="main.ms")
    zf.writeStr("version.txt", "1.0.0\n")
}

// 2. 列出归档内容
with zipfile.ZipFile("archive.zip", "r") as zf {
    for name := range zf.nameList() {
        info := zf.getInfo(name)
        fmt.printf("%-20s  %8d → %8d 字节\n",
            info.filename, info.fileSize, info.compressSize)
    }
}

// 3. 读取单个成员
with zipfile.ZipFile("archive.zip", "r") as zf {
    content := zf.read("version.txt")
    fmt.println(string(content))  // "1.0.0"
}

// 4. 流式读取大成员
with zipfile.ZipFile("archive.zip", "r") as zf {
    with zf.open("main.ms") as f {
        for line := range f.readLines() {
            fmt.print(string(line))
        }
    }
}

// 5. 解压所有成员到指定目录
with zipfile.ZipFile("archive.zip", "r") as zf {
    zf.extractAll(path="./output/")
}

// 6. 检查文件是否为 ZIP
fmt.println(zipfile.isZipfile("archive.zip"))  // true
fmt.println(zipfile.isZipfile("notes.txt"))    // false
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `zipfile.BadZipFile` | 文件不是合法 ZIP 格式，或归档数据已损坏（`Exception` 的子类） |
| `zipfile.LargeZipFile` | `allowZip64=false` 时归档大小或成员数量超出 ZIP 规范限制 |
| `KeyError` | `read()`/`getInfo()`/`open()` 引用了不存在的成员名 |
| `FileExistsError` | `mode="x"` 时目标文件已存在 |
| `OSError` | 文件不存在、权限不足或其他 I/O 错误 |
