# P12-T166 stdlib: hmac / secrets

> **状态**：⬜ 未开始

---

## 任务目标 / 背景

实现 `hmac`（基于哈希的消息认证码，RFC 2104）和 `secrets`（密码学安全随机数生成）模块。

---

## 前置依赖

| 任务号 | 说明 |
|---|---|
| P12-T163 | hashlib（MD5/SHA-1/SHA-256） |
| P12-T136 | os.urandom |

---

## 设计文档引用

| 文档 | 章节 |
|---|---|
| `stdlib/stdlib-hmac-secrets.md` | §1 模块 API |

---

## API 清单

```ms
// hmac
hmac.new(key, msg=b"", digestmod="sha256") → HMAC
hmac.digest(key, msg, digest) → bytes  // 一次性计算

h := hmac.new(b"secret", b"message", "sha256")
h.update(b"more data")
h.digest() → bytes
h.hexdigest() → str
h.copy() → HMAC
h.digestSize → int
h.name → str   // "hmac-sha256"

// 安全比较（时间恒定，防止时序攻击）
hmac.compareDigest(a, b) → bool
// 等同 == 但不暴露比较位置（constant-time）

// secrets
secrets.token_bytes(nbytes=32) → bytes    // nbytes 个随机字节
secrets.token_hex(nbytes=32) → str        // 十六进制字符串（长度 = 2*nbytes）
secrets.token_urlsafe(nbytes=32) → str    // URL-safe Base64
secrets.choice(seq) → elem                // 密码学安全的随机选择
secrets.randbelow(n) → int                // [0, n) 密码学安全随机整数
secrets.randbits(k) → int                 // k 位密码学安全随机整数
secrets.SystemRandom                      // 类（同 CPython）
```

---

## 实现要点

```c
// HMAC（RFC 2104）：
// ipad = 0x36 * blocksize; opad = 0x5C * blocksize
// HMAC(key, msg) = H((key ^ opad) || H((key ^ ipad) || msg))
// 若 len(key) > blocksize：key = H(key)（截断至摘要长度）
// 若 len(key) < blocksize：右侧补 0

typedef struct MsHMACObj {
  MsObject header;
  MsHashObj* inner;   // H((key ^ ipad) || msg）
  MsHashObj* outer;   // H((key ^ opad) || ...)
  uint8_t    digestSize;
} MsHMACObj;

// hmac_new：
// 1. 若 key 过长，先哈希
// 2. 初始化 inner = H(key ^ ipad)（已 update）
// 3. 初始化 outer = H(key ^ opad)（未 update）
// update(msg)：只 update inner
// digest()：outer.copy() → update(inner.digest()) → digest()

// compareDigest：time-constant XOR 比较
// len(a) != len(b) 时返回 false，但仍比较（避免泄露长度）
static bool compareDigestCt(const uint8_t* a, const uint8_t* b, size_t n) {
  uint8_t diff = 0;
  for (size_t i = 0; i < n; i++) diff |= a[i] ^ b[i];
  return diff == 0;
}

// secrets：全部基于 os.urandom（/dev/urandom 或 BCryptGenRandom）
// randbelow(n)：拒绝采样，避免模偏差
// token_urlsafe：os.urandom + base64.urlsafe_b64encode
```

---

## 验收标准（checklist）

- [ ] `hmac.digest(b"key", b"msg", "sha256").hex()` 等于 RFC 4231 测试向量。
- [ ] HMAC 更新等价：分块 update 与一次性计算结果相同。
- [ ] `compareDigest(a,a)` → true，`compareDigest(a,b)` → false（不同内容）。
- [ ] `compareDigest` 执行时间不依赖差异位置（时间恒定）。
- [ ] `secrets.token_bytes(32)` 每次调用返回不同值（熵足够）。
- [ ] `secrets.randbelow(100)` 分布均匀（chi-squared 通过，1000 次）。

---

## 测试用例（.ms）

```ms
import hmac, secrets, hashlib

// HMAC-SHA256（RFC 4231 Test Case 1）
key := bytes([0x0b] * 20)
msg := b"Hi There"
expected := "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"
result := hmac.digest(key, msg, "sha256").hex()
print(result == expected)   // true

// 增量 HMAC
h := hmac.new(b"secret", digestmod="sha256")
h.update(b"hello ")
h.update(b"world")
d1 := h.hexdigest()
d2 := hmac.digest(b"secret", b"hello world", "sha256").hex()
print(d1 == d2)   // true

// compareDigest（安全验证）
mac_a := hmac.digest(b"key", b"msg", "sha256")
mac_b := hmac.digest(b"key", b"msg", "sha256")
print(hmac.compareDigest(mac_a, mac_b))   // true

// secrets
tok := secrets.token_hex(16)
print(len(tok))    // 32（16字节的十六进制）
print(type(tok))   // str

// 密码生成示例
import string
alphabet := string.ascii_letters + string.digits + "!@#$%"
password := "".join([secrets.choice(alphabet) for _ in range(16)])
print(len(password))  // 16
```
