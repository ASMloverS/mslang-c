# P12-T168 stdlib: zipfile

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `zipfile` 模块（对齐 `stdlib/zipfile.md`）：读写 ZIP 文件格式（PKWARE spec），利用 T167 DEFLATE 引擎，支持 STORED 和 DEFLATE 两种压缩方式。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T167 | gzip/DEFLATE 引擎（复用 deflate/inflate） |
| P12-T134 | io 模块 |

---

## API 清单

```ms
// 打开/创建 ZIP 文件
zf := zipfile.ZipFile(file, mode="r")
// mode: "r"=读 "w"=写（覆盖） "a"=追加 "x"=排他创建
// file 可为路径字符串或 file-like 对象

zf.namelist() → list[str]        // 所有条目名
zf.infolist() → list[ZipInfo]    // 所有条目元数据
zf.getinfo(name) → ZipInfo       // 单个条目元数据

// 读
zf.read(name) → bytes            // 读取文件内容
zf.open(name, mode="r") → file   // 返回可读文件对象

// 写
zf.write(filename, arcname=nil, compress_type=nil)
zf.writestr(zinfo_or_name, data, compress_type=nil)
// compress_type: zipfile.ZIP_STORED(0) 或 ZIP_DEFLATED(8)

// 上下文管理器
with zipfile.ZipFile("arch.zip", "w") as zf:
    zf.writestr("hello.txt", "hello world")

zf.extractall(path=".", members=nil)
zf.extract(member, path=".", pwd=nil)
zf.close()
zf.testzip() → str|nil   // nil=完整, str=第一个损坏文件名

// ZipInfo 属性
info.filename    info.file_size    info.compress_size
info.compress_type   info.date_time   info.CRC    info.comment

// 便捷函数
zipfile.is_zipfile(filename) → bool   // 检查 magic bytes
```

---

## 实现要点

```c
// ZIP 文件格式（PKWARE Application Note）：
// [Local File Header + File Data] × N
// [Data Descriptor（可选）]
// Central Directory：描述所有条目（含偏移量）
// End of Central Directory Record：定位 CD 起始位置

// Local File Header (30 bytes + variable):
// sig=0x04034b50 version_needed flags compression mtime mdate crc32
// compressed_size uncompressed_size filename_len extra_len
// [filename] [extra]

// Central Directory Entry (46 bytes + variable):
// sig=0x02014b50 version_made version_needed flags compression...
// crc32 compressed_size uncompressed_size ... local_header_offset
// [filename] [extra] [comment]

// End of Central Directory (22 bytes + variable):
// sig=0x06054b50 disk_number cd_disk cd_entries total_entries
// cd_size cd_offset comment_len [comment]

// 写入流程：
// 1. 写各 Local File Header + 压缩数据（deflate 或 stored）
// 2. 所有文件写完后，从文件开头重写 CRC/size（或用 data descriptor）
// 3. 写 Central Directory + End of Central Directory

typedef struct ZipEntry {
  char*    filename;
  uint32_t crc32;
  uint32_t compressed_size;
  uint32_t uncompressed_size;
  uint16_t compress_type;   // 0=stored 8=deflated
  uint32_t local_offset;    // 本地头在文件中的偏移
  uint32_t date_time;       // packed DOS date/time
} ZipEntry;
```

---

## 验收标准（checklist）

- [ ] 创建 ZIP 写入多文件，然后读回内容一致。
- [ ] `zf.testzip()` 返回 nil（数据完整）。
- [ ] 生成的 .zip 文件可被系统 unzip 工具解压。
- [ ] 能读取系统 zip 工具生成的标准 .zip 文件。
- [ ] DEFLATE 压缩的条目正确解压（复用 T167 inflate）。
- [ ] `extractall` 正确恢复文件树。

---

## 测试用例（.ms）

```ms
import zipfile, io

// 内存 round-trip
buf := io.BytesIO()
with zipfile.ZipFile(buf, "w") as zf:
    zf.writestr("hello.txt", "Hello, World!")
    zf.writestr("sub/data.txt", "Data inside subfolder")

buf.seek(0)
with zipfile.ZipFile(buf, "r") as zf:
    print(zf.namelist())   // ["hello.txt", "sub/data.txt"]
    print(zf.read("hello.txt"))  // b"Hello, World!"
    info := zf.getinfo("hello.txt")
    print(info.file_size)   // 13

// 文件写入与解压
with zipfile.ZipFile("/tmp/test.zip", "w") as zf:
    zf.write("/tmp/somefile.txt", "test.txt", zipfile.ZIP_DEFLATED)

with zipfile.ZipFile("/tmp/test.zip", "r") as zf:
    zf.extractall("/tmp/extracted/")
    print(zf.testzip())  // nil（无损坏）

// is_zipfile
print(zipfile.is_zipfile("/tmp/test.zip"))  // true
```
