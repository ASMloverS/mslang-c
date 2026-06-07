# P12-T195 stdlib: io（StringIO / BytesIO 扩展）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

完善 `io` 模块（T134 已实现基础），补充缺失的 API：完整 `StringIO`/`BytesIO`（seek/tell/getvalue）、`BufferedReader`/`BufferedWriter`、`TextIOWrapper`。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T134 | io 基础（MsStringIOObj/MsBytesIOObj 骨架） |

---

## API 清单

```ms
// StringIO（内存字符串流）
buf := io.StringIO("initial content")
buf := io.StringIO()

buf.write("more text") → int     // 返回写入字节数
buf.read(n=-1) → str             // 读 n 字符（-1=全部）
buf.readline() → str
buf.readlines() → list[str]
buf.writelines(lines)            // 写入行列表
buf.seek(pos, whence=0)          // 0=起始 1=当前 2=末尾
buf.tell() → int
buf.getvalue() → str             // 返回全部内容（不改变 pos）
buf.truncate(size=nil)           // 截断到 size（nil=当前 pos）
buf.readable() → true
buf.writable() → true
buf.seekable() → true
buf.closed → bool
buf.close()

// BytesIO（内存字节流）
bbuf := io.BytesIO(b"initial")
bbuf.write(b"more") → int
bbuf.read(n=-1) → bytes
bbuf.seek(pos, whence=0)
bbuf.tell() → int
bbuf.getvalue() → bytes
bbuf.getbuffer() → memoryview    // 零拷贝视图

// BufferedReader（带缓冲的可读流）
br := io.BufferedReader(raw, buffer_size=8192)
br.read(n=-1) → bytes
br.readline() → bytes
br.readlines() → list[bytes]
br.peek(n=0) → bytes   // 预读不推进 pos
br.read1(n=-1) → bytes // 最多一次系统调用

// BufferedWriter（带缓冲的可写流）
bw := io.BufferedWriter(raw, buffer_size=8192)
bw.write(data) → int
bw.flush()              // 强制写入底层
bw.close()              // flush + close

// TextIOWrapper（字节流 → 文本流适配器）
tw := io.TextIOWrapper(binary_stream, encoding="utf-8", errors="strict",
                       newline=nil, line_buffering=false, write_through=false)
tw.read(n=-1) → str
tw.readline() → str
tw.write(s) → int
tw.encoding → str
tw.errors → str
tw.newlines → str|tuple|nil  // 检测到的换行符类型

// 抽象基类（用于类型检查）
io.RawIOBase
io.BufferedIOBase
io.TextIOBase
io.IOBase
```

---

## 实现要点

```c
// StringIO 完整实现：
// 内部：MsWriter 字节缓冲（存储 UTF-8）+ pos（字节偏移）
// getvalue()：返回整个缓冲（不受 pos 影响）
// seek(pos, whence=0)：
//   whence=0：绝对位置
//   whence=1：当前位置 + pos
//   whence=2：末尾位置 + pos（通常 pos=0）
// truncate(size)：截断缓冲，pos 不变（若 size < pos 则 pos=size）

// BytesIO：与 StringIO 类似，但操作字节
// getbuffer()：返回指向内部 buf 的零拷贝视图（memoryview 对象）
// 注意：getbuffer() 期间不能 resize（需要固定缓冲）

typedef struct MsBytesIOObj {
  MsObject  header;
  uint8_t*  buf;
  size_t    len;
  size_t    cap;
  size_t    pos;
  bool      exported;  // getbuffer() 后设为 true，阻止 write
} MsBytesIOObj;

// BufferedReader：
// 维护 read_buf[buffer_size]，read_pos, read_end
// read(n)：先从缓冲读，不足则调用 raw.readinto 填充
// peek(n)：返回缓冲中的数据不推进 pos（调用 raw.readinto 填充但不移动 pos）

// TextIOWrapper：
// write(s)：s.encode(encoding) → write to binary stream
// read(n=-1)：binary.read() → decode(encoding)
// readline()：读到 '\n'（或 '\r\n'）并处理 newline 转换
// newline 处理：universal newlines（\r\n \r \n 均转为 \n）

// 编码错误处理（errors="strict"/"ignore"/"replace"/"surrogateescape"）
```

---

## 验收标准（checklist）

- [ ] `StringIO.seek(0)` 后 `read()` 重新读取全部内容。
- [ ] `StringIO.getvalue()` 不受 `seek` 影响，始终返回全部内容。
- [ ] `BytesIO.getbuffer()` 提供零拷贝视图。
- [ ] `BufferedReader.peek()` 不推进读取位置。
- [ ] `TextIOWrapper` 正确处理 UTF-8 解码错误（按 errors 参数）。
- [ ] `TextIOWrapper` 的 newline="\r\n" 写入时转换换行符。

---

## 测试用例（.ms）

```ms
import io

// StringIO
buf := io.StringIO()
buf.write("hello ")
buf.write("world")
print(buf.tell())      // 11（字节）
print(buf.getvalue())  // "hello world"
buf.seek(0)
print(buf.read())      // "hello world"
buf.seek(6)
print(buf.read())      // "world"

// BytesIO
bbuf := io.BytesIO()
bbuf.write(b"\x01\x02\x03")
bbuf.seek(0)
print(bbuf.read(1))    // b'\x01'
print(bbuf.tell())     // 1

// BytesIO round-trip 通过 gzip
import gzip
data := b"hello world" * 100
gz_buf := io.BytesIO()
with gzip.open(gz_buf, "wb") as f:
    f.write(data)
gz_buf.seek(0)
with gzip.open(gz_buf, "rb") as f:
    result := f.read()
print(result == data)  // true

// TextIOWrapper
raw := io.BytesIO()
tw := io.TextIOWrapper(raw, encoding="utf-8")
tw.write("Hello, 世界!\n")
tw.flush()
raw.seek(0)
tw2 := io.TextIOWrapper(raw, encoding="utf-8")
print(tw2.readline())   // "Hello, 世界!\n"

// BufferedReader
import os
r, w := os.pipe()
os.write(w, b"test data for buffered read")
os.close(w)
br := io.BufferedReader(os.fdopen(r, "rb"))
print(br.peek(4))   // b"test"（预读，pos 不变）
print(br.read(4))   // b"test"（实际读取）
br.close()
```
