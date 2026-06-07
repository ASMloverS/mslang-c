# P12-T169 stdlib: tarfile

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `tarfile` 模块（对齐 `stdlib/tarfile.md`）：读写 tar 归档格式（POSIX.1-2001 / GNU tar 兼容），支持 .tar / .tar.gz / .tar.bz2（BZ2 暂不实现，标注 NotImplemented）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T167 | gzip（.tar.gz 支持） |
| P12-T134 | io 模块 |
| P12-T135 | os.path |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-tarfile.md` | §1 模块 API |

---

## API 清单

```ms
// 打开 tar 归档
tarfile.open(name, mode="r") → TarFile
tarfile.open(name, "r:gz")   // .tar.gz 读
tarfile.open(name, "w:gz")   // .tar.gz 写
tarfile.open(name, "r:*")    // 自动检测压缩

// mode 字符串：
// "r"/"r:"=无压缩  "r:gz"=gzip  "r:bz2"=bzip2（保留，NotImplemented）
// "w"/"w:"=无压缩  "w:gz"=gzip
// "a"=追加（仅无压缩 tar）

tf := tarfile.open("archive.tar.gz", "w:gz")
tf.add(name, arcname=nil, recursive=true)   // 添加文件/目录
tf.addfile(tarinfo, fileobj=nil)            // 从 TarInfo + 数据添加
tf.close()

tf := tarfile.open("archive.tar.gz", "r:gz")
tf.getnames() → list[str]
tf.getmembers() → list[TarInfo]
tf.getmember(name) → TarInfo
tf.extractall(path=".")
tf.extract(member, path=".")
tf.extractfile(member) → file|nil   // nil 表示目录/链接
tf.close()

// TarInfo 属性
ti.name       ti.size       ti.mode     ti.mtime
ti.type       // REGTYPE/DIRTYPE/SYMTYPE/LNKTYPE/CHRTYPE/BLKTYPE/FIFOTYPE
ti.linkname   ti.uid   ti.gid   ti.uname   ti.gname
ti.isdir()    ti.isreg()    ti.issym()

// 上下文管理器支持
with tarfile.open("a.tar", "w") as tf:
    tf.add("myfile.txt")
```

---

## 实现要点

```c
// tar 块大小：512 字节（BLOCKSIZE）
// 头部（512 字节 POSIX ustar 格式）：
// [100] name  [8] mode  [8] uid  [8] gid
// [12] size（八进制字符串）  [12] mtime  [8] checksum
// [1] typeflag  [100] linkname
// [6] magic="ustar"  [2] version="00"
// [32] uname  [32] gname  [8] devmajor  [8] devminor
// [155] prefix  [12] padding

// 写入步骤：
// 1. 构建 TarInfo（header block）
// 2. 计算校验和（header 字节和，校验和字段置空格）
// 3. 写 512 字节 header
// 4. 写数据（对齐到 512 字节边界，末尾填 0）
// 结尾：两个全零 512 字节块

// 长文件名（> 100 字节）：
// GNU tar：GNUTYPE_LONGNAME (0x4c='L') 前置头
// POSIX pax：pax 扩展属性

// .tar.gz：将 tf 的读写管道化到 gzip 流
// 写：gzip.GzipWriter 包裹文件，tar 块写入 gzip 流
// 读：gzip.GzipReader 包裹文件，从中读取 tar 块

// checksum：
// sum = 0; header[148..155] = "        "（8个空格）
// for byte in header: sum += byte
// header[148..155] = "%06o\0 " % sum

typedef struct TarHeader {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char chksum[8];
  char typeflag;
  char linkname[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char padding[12];
} TarHeader;   // sizeof = 512
```

---

## 验收标准（checklist）

- [ ] 创建 .tar.gz，内含多文件和子目录，能被系统 tar 命令解压。
- [ ] 能读取系统 tar 命令创建的标准 .tar 和 .tar.gz 文件。
- [ ] 长文件名（> 100 字节路径）正确处理。
- [ ] `tf.extractall()` 恢复文件和权限（mode）。
- [ ] 目录递归 add：`tf.add("somedir")` 包含所有子文件。
- [ ] `.tar.gz` 使用 gzip level 9 压缩。

---

## 测试用例（.ms）

```ms
import tarfile, os

// 写入 .tar.gz
with tarfile.open("/tmp/test.tar.gz", "w:gz") as tf:
    tf.add("/tmp/hello.txt")   // 添加单个文件
    tf.add("/tmp/mydir", arcname="subdir")  // 目录（递归）

// 读取并检查内容
with tarfile.open("/tmp/test.tar.gz", "r:gz") as tf:
    print(tf.getnames())   // ["hello.txt", "subdir/...", ...]

    // 提取单个文件
    f := tf.extractfile("hello.txt")
    if f { print(f.read()) }

    // 全部解压
    tf.extractall("/tmp/extracted/")

// 内存中创建 tarball
import io
buf := io.BytesIO()
with tarfile.open(fileobj=buf, mode="w") as tf:
    data := b"file contents"
    ti := tarfile.TarInfo(name="data.txt")
    ti.size = len(data)
    tf.addfile(ti, io.BytesIO(data))

buf.seek(0)
with tarfile.open(fileobj=buf, mode="r") as tf:
    print(tf.getnames())   // ["data.txt"]
```
