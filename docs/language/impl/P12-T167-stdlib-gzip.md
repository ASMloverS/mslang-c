# P12-T167 stdlib: gzip（自实现 DEFLATE）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `gzip` 模块（对齐 `stdlib/gzip.md`），核心是自实现 **DEFLATE** 压缩算法（RFC 1951）+ gzip 文件格式（RFC 1952）。零外部依赖（不使用 zlib）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T058 | MsBytesObj |
| P12-T134 | io 模块（文件读写） |
| P12-T163 | hashlib（gzip 使用 CRC-32） |

---

## API 清单

```ms
// 文件读写
gzip.open(filename, mode="rb", compresslevel=9) → GzipFile
// mode: "rb","wb","rt","wt"（文本模式自动解码）

f := gzip.open("file.gz", "wb")
f.write(b"hello world")
f.close()

f := gzip.open("file.gz", "rb")
data := f.read()
f.close()

// 内存压缩/解压
gzip.compress(data, compresslevel=9) → bytes    // 压缩（含 gzip 头）
gzip.decompress(data) → bytes                   // 解压

// GzipFile 方法（类文件接口）
f.read(n=-1) → bytes
f.readline() → bytes
f.readlines() → list[bytes]
f.write(data)
f.seek(offset, whence=0)    // 仅解压模式
f.tell() → int

// compresslevel: 0=不压缩 1=最快 9=最小（默认 9）
```

---

## 实现要点

```c
// === DEFLATE 编码（压缩）===
// Block 类型：00=无压缩 01=固定 Huffman 10=动态 Huffman
// 压缩流程：
// 1. LZ77：查找历史匹配（最多 32768 字节窗口，最长 258 字节）
//    使用哈希链：hash(buf[i..i+3]) → 最近匹配位置
// 2. Huffman 编码：
//    - 字面/长度编码（0-285）
//    - 距离编码（0-29）
//    - 动态 Huffman：先收集频率，然后构造最优树（限制深度 ≤ 15）

// === DEFLATE 解码（解压）===
// 读 block header → 按类型处理
// 动态 Huffman：先解码头部（HLIT+HDIST+HCLEN），构造树，然后解码

// === CRC-32（gzip 校验和）===
// IEEE 多项式 0xEDB88320（bit-reversed）
// 预计算 256 项查找表，O(1) 每字节

// gzip 文件格式：
// Header: ID1=0x1f ID2=0x8b CM=8 FLG MTIME XFL OS
// Compressed data（DEFLATE）
// Trailer: CRC32[4] ISIZE[4]（原始数据大小 mod 2^32）

typedef struct MsDeflateCtx {
  uint8_t* out;
  size_t   outlen, outcap;
  // LZ77 哈希链
  uint16_t head[65536];  // 哈希表头（32KB 窗口）
  uint16_t prev[32768];  // 链表
  uint8_t* window;       // 历史窗口
  // 当前 block 的字面/长度 + 距离频率
  uint32_t lit_freq[288];
  uint32_t dist_freq[30];
  // 位写入缓冲
  uint32_t bitbuf;
  int      bitcnt;
} MsDeflateCtx;
```

---

## 验收标准（checklist）

- [ ] `gzip.compress(b"hello")` 产生有效 gzip 数据（可被系统 gunzip 解压）。
- [ ] `gzip.decompress(gzip.compress(data)) == data`（round-trip）。
- [ ] 高度可压缩数据（全零 1MB）压缩比 > 99%。
- [ ] 随机数据（1MB）压缩后体积不超过原始 + 20 字节（不膨胀过多）。
- [ ] `gzip.open` 读取系统生成的 .gz 文件正确。
- [ ] compresslevel=1 比 compresslevel=9 快，但 size 更大。

---

## 测试用例（.ms）

```ms
import gzip, os

// 内存 round-trip
data := b"Hello, World! " * 1000
compressed := gzip.compress(data)
decompressed := gzip.decompress(compressed)
print(decompressed == data)   // true
print(len(compressed), "<", len(data))   // 压缩更小

// 文件读写
with gzip.open("/tmp/test.gz", "wb") as f:
    f.write(b"test data " * 100)

with gzip.open("/tmp/test.gz", "rb") as f:
    result := f.read()
print(len(result))    // 1000

// 压缩级别比较
import time
big := b"X" * 1_000_000
t0 := time.now()
c9 := gzip.compress(big, compresslevel=9)
t1 := time.now()
c1 := gzip.compress(big, compresslevel=1)
t2 := time.now()
print("level 9:", len(c9), "bytes,", t1-t0, "ms")
print("level 1:", len(c1), "bytes,", t2-t1, "ms")
```

---

## Benchmark

```ms
import gzip, time

// 1MB 文本压缩/解压
text := ("The quick brown fox jumps over the lazy dog. " * 2000).encode()
t0 := time.now()
c := gzip.compress(text)
t1 := time.now()
d := gzip.decompress(c)
t2 := time.now()
print("compress 1MB:", t1-t0, "ms, ratio:", len(c)/len(text))
print("decompress:", t2-t1, "ms")
// 目标：compress < 200ms, decompress < 50ms（level 9）
```
