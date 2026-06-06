# P12-T165 stdlib: hashlib（SHA-3 / BLAKE2）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `hashlib` 的 SHA-3（Keccak 海绵构造）和 BLAKE2b/BLAKE2s，完成 hashlib 模块。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T164 | hashlib SHA-256/SHA-224 |

---

## API 清单（本任务新增）

```ms
// SHA-3 家族（Keccak-p[1600, 24]）
hashlib.sha3_224(data=b"") → hash_obj  // 28 字节
hashlib.sha3_256(data=b"") → hash_obj  // 32 字节
hashlib.sha3_384(data=b"") → hash_obj  // 48 字节
hashlib.sha3_512(data=b"") → hash_obj  // 64 字节
hashlib.shake_128(data=b"") → xof      // 可变长度输出（XOF）
hashlib.shake_256(data=b"") → xof

// XOF 额外方法（SHAKE）
xof.digest(length) → bytes    // 指定长度摘要（而非固定长）

// BLAKE2（RFC 7693）
hashlib.blake2b(data=b"", digest_size=64, key=b"", salt=b"", person=b"")
// key: 0-64 字节（带密钥 MAC 模式）
// salt: 0-16 字节
// person: 0-16 字节

hashlib.blake2s(data=b"", digest_size=32, key=b"", salt=b"", person=b"")
// key: 0-32 字节

// hashlib.algorithms_available → set（已实现的算法名称集合）
// hashlib.algorithms_guaranteed → set（跨平台保证可用的）
```

---

## 实现要点

```c
// === SHA-3（Keccak）===
// 状态：5×5 个 uint64_t（200 字节，1600 位）
// Keccak-f[1600] 24 轮置换（θ ρ π χ ι 5步操作）

typedef struct KeccakState {
    uint64_t A[5][5];   // 1600 位状态
    uint8_t  buf[136];  // 最大速率 136 字节（SHA3-224 使用 144，但 SHAKE128 136）
    uint32_t buflen;
    uint32_t rate;      // 字节（1600-capacity）/ 8
    uint8_t  suffix;    // 0x06=SHA3 0x1f=SHAKE
} KeccakState;

// θ: a[x][y] ^= parity(col(x-1)) ^ ROL(parity(col(x+1)), 1)
// ρ: 各通道按固定偏移循环左移
// π: 位置重排（y,x) → (x, 2x+3y) mod 5
// χ: a[x] ^= (~a[x+1]) & a[x+2]（非线性）
// ι: 轮常量异或

// === BLAKE2b ===
// 状态：8 个 uint64_t（Blake2b）
// 轮数 12，压缩函数 G 混合
// 初始化接受参数块（key/salt/person/fanout/depth/leaf_size...）
// Blake2s：类似但使用 uint32_t + 10 轮

typedef struct Blake2bState {
    uint64_t h[8];      // 链接值
    uint64_t t[2];      // 计数器（128 位）
    uint64_t f[2];      // 末块标志
    uint8_t  buf[128];  // 输入缓冲（128 字节块）
    uint32_t buflen;
    uint8_t  outlen;    // 输出字节数（1-64）
} Blake2bState;

// G 函数：
// a += b + m[i]; d ^= a; d = ROTR64(d,32)
// c += d; b ^= c; b = ROTR64(b,24)
// a += b + m[j]; d ^= a; d = ROTR64(d,16)
// c += d; b ^= c; b = ROTR64(b,63)
```

---

## 验收标准（checklist）

- [ ] `sha3_256(b"abc").hexdigest()` → `"3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532"`。
- [ ] `sha3_512(b"").hexdigest()` 正确（NIST 标准向量）。
- [ ] SHAKE-128 `xof.digest(32)` 可生成 32 字节，`xof.digest(64)` 生成 64 字节。
- [ ] `blake2b(b"abc").hexdigest()` → 正确标准向量。
- [ ] `blake2b(b"key", key=b"secret")` 带密钥 MAC 正确。
- [ ] SHA-3 1MB 数据 < 200ms，BLAKE2b 1MB < 100ms。

---

## 测试用例（.ms）

```ms
import hashlib

// SHA-3
print(hashlib.sha3_256(b"abc").hexdigest())
// 3a985da74fe225b2045c172d6bd390bd855f086e3e9d525b46bfe24511431532

// SHAKE（可变输出长度）
xof := hashlib.shake_128(b"hello")
print(len(xof.digest(100)))   // 100
print(len(xof.digest(10)))    // 10（同一 xof 可多次调用）

// BLAKE2b
print(hashlib.blake2b(b"").hexdigest())
// 786a02f742015903c6c6fd852552d272912f4740e15847618a86e217f71f5419
// d60cee29564d2a1b8014085a3520159027571e30

// BLAKE2b MAC（带密钥）
h := hashlib.blake2b(b"message", key=b"secretkey")
print(len(h.digest()))   // 64

// 可用算法
print(hashlib.algorithms_available)
// {"md5","sha1","sha224","sha256","sha384","sha512",
//  "sha3_224","sha3_256","sha3_384","sha3_512",
//  "shake_128","shake_256","blake2b","blake2s"}
```
