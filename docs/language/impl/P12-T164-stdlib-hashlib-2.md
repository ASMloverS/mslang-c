# P12-T164 stdlib: hashlib（SHA-256 / SHA-224）

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `hashlib` 的 SHA-256 和 SHA-224 算法（SHA-2 家族，32 位版本）。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T163 | hashlib MD5/SHA-1（MsHashObj 框架） |

---

## API 清单（本任务新增）

```ms
hashlib.sha256(data=b"") → hash_obj
hashlib.sha224(data=b"") → hash_obj
hashlib.new("sha256")
hashlib.new("sha224")

// SHA-256 摘要长度：32 字节（64 hex）
// SHA-224 摘要长度：28 字节（56 hex）
// 相同接口：update / digest / hexdigest / copy
```

---

## 实现要点

```c
// SHA-256：FIPS 180-4 标准
// 状态：8 个 uint32_t（h0..h7）
// 初始值：前 8 个素数的平方根小数部分
// 轮常量 K[64]：前 64 个素数的立方根小数部分

static const uint32_t SHA256_K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, ...
};

typedef struct Sha256State {
    uint32_t state[8];
    uint64_t count;
    uint8_t  buf[64];
    uint32_t buflen;
} Sha256State;

// 每轮操作：
// Ch(e,f,g) = (e & f) ^ (~e & g)
// Maj(a,b,c) = (a & b) ^ (a & c) ^ (b & c)
// Σ0(a) = ROTR(a,2) ^ ROTR(a,13) ^ ROTR(a,22)
// Σ1(e) = ROTR(e,6) ^ ROTR(e,11) ^ ROTR(e,25)
// σ0(x) = ROTR(x,7) ^ ROTR(x,18) ^ SHR(x,3)
// σ1(x) = ROTR(x,17) ^ ROTR(x,19) ^ SHR(x,10)

// SHA-224：与 SHA-256 相同算法，不同初始状态，截断输出为 224 位（28 字节）
// 初始值：前 9..16 个素数的平方根小数部分

// 性能优化：
// 1. 展开内层 64 步循环（unrolled）
// 2. 本机字节序避免逐字节拷贝（检测 __BYTE_ORDER__）
```

---

## 验收标准（checklist）

- [ ] `sha256(b"abc").hexdigest()` → `"ba7816bf8f01cfea414140de5dae2ec73b00361bbef0469348423f656fce6208"`。
- [ ] `sha224(b"abc").hexdigest()` → `"23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7"`。
- [ ] 分块 update 与一次性 update 结果相同。
- [ ] 1MB 数据 SHA-256 时间 < 100ms。
- [ ] `copy()` 正确分叉计算。

---

## 测试用例（.ms）

```ms
import hashlib

// SHA-256 标准向量
print(hashlib.sha256(b"").hexdigest())
// e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855

print(hashlib.sha256(b"abc").hexdigest())
// ba7816bf8f01cfea414140de5dae2ec73b00361bbef0469348423f656fce6208

// SHA-224
print(hashlib.sha224(b"abc").hexdigest())
// 23097d223405d8228642a477bda255b32aadbce4bda0b3f7e36c9da7

// 增量
h := hashlib.sha256()
for chunk in [b"The quick ", b"brown fox ", b"jumps over ", b"the lazy dog"] {
    h.update(chunk)
}
print(h.hexdigest())
// d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592
```
