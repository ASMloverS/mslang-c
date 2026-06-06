# P12-T134 stdlib: io

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `io` 模块：抽象 I/O 基类（`IOBase`/`RawIOBase`/`BufferedIOBase`/`TextIOBase`）和具体实现（`FileIO`、`BufferedReader/Writer`、`StringIO`、`BytesIO`）。`open()` 内置函数（T104）在本模块完善。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P8-T104 | open() 基础（MsFileObj） |
| P4-T058 | MsBytesObj |
| P4-T057 | MsStrObj |

---

## API 清单

```ms
// 对齐 stdlib/io.md
io.StringIO(initial="")   // 内存字符串缓冲
io.BytesIO(initial=b"")   // 内存字节缓冲

// StringIO/BytesIO 方法
io.read(size=-1) → str/bytes
io.write(data) → int      // 写入字节数
io.seek(pos, whence=0)    // 定位
io.tell() → int           // 当前位置
io.getvalue() → str/bytes // 获取全部内容
io.close()
io.closed → bool

// 文件读写（扩展 T104 的 MsFileObj）
read(size=-1) → str       // 文本模式
readline() → str
readlines() → list[str]
write(s) → int
writelines(lines)
flush()
truncate(size=None)
fileno() → int
isatty() → bool
```

---

## 实现要点

### StringIO

```c
typedef struct MsStringIOObj {
    MsObject header;
    MsStrObj** chunks;     // 写入缓冲区（延迟拼接）
    uint32_t   chunkCount;
    uint32_t   pos;        // 读取位置（字节偏移）
    bool       closed;
} MsStringIOObj;

// getvalue(): 将 chunks 拼接为单一字符串
// read(n): 从 pos 读取 n 字节（-1 = 全部）
// write(s): 追加 chunk
// seek(pos, whence): 调整 pos（SEEK_SET/CUR/END）
```

### BytesIO

```c
// 类似 StringIO，但内部是 uint8_t 缓冲区
// write(b) 追加 bytes，getvalue() 返回 bytes 对象
```

---

## 验收标准（checklist）

- [ ] `io.StringIO()` 支持 read/write/seek/tell/getvalue。
- [ ] `io.BytesIO(b"hello")` → read() 返回 `b"hello"`。
- [ ] `with io.StringIO() as f: f.write("x")` → 正常关闭。
- [ ] 文件对象（open()）支持 readline/readlines/writelines。
- [ ] 二进制模式 `open(f,"rb")` → read() 返回 bytes。

---

## 测试用例（.ms）

```ms
import io

// StringIO
buf := io.StringIO()
buf.write("hello ")
buf.write("world")
buf.seek(0)
print(buf.read())    // hello world
print(buf.getvalue())  // hello world

// BytesIO
b := io.BytesIO(bytes([1,2,3]))
print(b.read(2))    // b'\x01\x02'
print(b.tell())     // 2

// 文件读写
with open("/tmp/io_test.txt", "w") as f {
    f.writelines(["line1\n", "line2\n"])
}
with open("/tmp/io_test.txt") as f {
    lines := f.readlines()
    print(lines)  // ["line1\n", "line2\n"]
}
```

---

## Benchmark

```ms
// 1M StringIO 写入
import io, time
buf := io.StringIO()
t0 := time.now()
for i in range(1_000_000) { buf.write("x") }
t1 := time.now()
print("1M writes:", t1-t0, "ms")  // 目标 < 500ms
```

---

## 风险与边界

- **UTF-8 边界**：`StringIO.read(n)` 中 n 是字符数（codepoints），不是字节数；需要 UTF-8 解码。
