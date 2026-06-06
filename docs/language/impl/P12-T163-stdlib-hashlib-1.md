# P12-T163 stdlib: hashlib（MD5 / SHA-1）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `hashlib` 模块的 MD5 和 SHA-1 算法（对齐 `stdlib/hashlib.md`），全自实现，零外部依赖。提供统一的 Hash 对象接口。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P4-T058 | MsBytesObj |

---

## API 清单（本任务范围）

```ms
// 统一接口
h := hashlib.new("md5")
h := hashlib.new("sha1")
hashlib.md5(data=b"")    // 快捷方式
hashlib.sha1(data=b"")

// 操作
h.update(data)            // data 可为 bytes 或 str（编码为 UTF-8）
h.digest() → bytes        // 当前哈希值（二进制）
h.hexdigest() → str       // 十六进制字符串
h.copy() → h              // 复制当前状态（用于分叉计算）
h.digest_size → int       // 摘要字节长度（MD5=16, SHA1=20）
h.block_size → int        // 块大小（MD5=64, SHA1=64）
h.name → str              // 算法名称

// 便捷函数
hashlib.md5(b"hello").hexdigest()
// → "5d41402abc4b2a76b9719d911017c592"
hashlib.sha1(b"hello").hexdigest()
// → "aaf4c61ddcc5e8a2dabede0f3b482cd9aea9434d"
```

---

## 实现要点

```c
// === MD5 实现 ===
// RFC 1321 标准实现
// 状态：4 个 uint32_t（a,b,c,d）
// 512 位块处理（16 个 uint32_t）
// 填充：消息末追加 1 位，然后 0 位，最后 64 位小端长度
// 4 轮，每轮 16 步，使用 F/G/H/I 非线性函数

typedef struct Md5State {
    uint32_t state[4];   // a,b,c,d
    uint64_t count;      // 总字节数
    uint8_t  buf[64];    // 当前未处理块
    uint32_t buflen;     // buf 中字节数
} Md5State;

static void md5_compress(uint32_t state[4], const uint8_t block[64]);

// === SHA-1 实现 ===
// RFC 3174 标准实现
// 状态：5 个 uint32_t（h0..h4）
// 512 位块处理
// 80 步，4 轮（FGHI 函数），使用循环左移

typedef struct Sha1State {
    uint32_t state[5];   // h0..h4
    uint64_t count;      // 位数
    uint8_t  buf[64];
    uint32_t buflen;
} Sha1State;

// MsHashObj：多态（通过 algorithm 枚举 + union）
typedef struct MsHashObj {
    MsObject header;
    int      algorithm;  // HASH_MD5, HASH_SHA1, ...
    union {
        Md5State  md5;
        Sha1State sha1;
    };
} MsHashObj;

// h.copy()：深拷贝整个 union（包括 buf 状态）
```

---

## 验收标准（checklist）

- [ ] `hashlib.md5(b"").hexdigest()` → `"d41d8cd98f00b204e9800998ecf8427e"`。
- [ ] `hashlib.md5(b"abc").hexdigest()` → `"900150983cd24fb0d6963f7d28e17f72"`。
- [ ] `hashlib.sha1(b"abc").hexdigest()` → `"a9993e364706816aba3e25717850c26c9cd0d89d"`。
- [ ] 增量 update：分块更新与一次性更新结果相同。
- [ ] `h.copy()` 后对两个对象各自 update，结果独立。
- [ ] 大数据（1MB）哈希时间 < 50ms。

---

## 测试用例（.ms）

```ms
import hashlib

// MD5
h := hashlib.md5()
h.update(b"hello ")
h.update(b"world")
print(h.hexdigest())   // 3e25960a79dbc69b674cd4ec67a72c62

// 与一次性等价
print(hashlib.md5(b"hello world").hexdigest())  // 同上

// SHA-1
print(hashlib.sha1(b"The quick brown fox jumps over the lazy dog").hexdigest())
// 2fd4e1c67a2d28fced849ee1bb76e7391b93eb12

// copy 分叉
h := hashlib.md5(b"foo")
h2 := h.copy()
h.update(b"bar")
h2.update(b"baz")
print(h.hexdigest())   // hash("foobar")
print(h2.hexdigest())  // hash("foobaz")（不同）

// 大文件模拟
h := hashlib.md5()
chunk := b"x" * 65536
for _ in range(16) { h.update(chunk) }  // 1MB
print(len(h.hexdigest()))  // 32
```

---

## Benchmark

```ms
import hashlib, time

data := b"A" * 1_000_000  // 1MB
t0 := time.now()
hashlib.md5(data)
t1 := time.now()
print("MD5 1MB:", t1-t0, "ms")   // 目标 < 50ms

t0 = time.now()
hashlib.sha1(data)
t1 = time.now()
print("SHA1 1MB:", t1-t0, "ms")  // 目标 < 50ms
```
