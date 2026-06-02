# hashlib — 加密哈希摘要

```ms
import hashlib
```

## 概述

提供 MD5、SHA-1、SHA-2 家族（SHA-224/256/384/512）、SHA-3 家族（SHA3-224/256/384/512）
以及 BLAKE2（blake2b/blake2s）等加密哈希算法。参考 Python hashlib 模块设计。

哈希对象支持增量更新（`update` → `hexdigest`），适合对大文件进行流式哈希，
无需一次性将整个文件载入内存。

所有构造函数均接受可选的初始 `data` 参数（`bytes` 或 `str`，`str` 按 UTF-8 编码），
等效于构造后立即调用 `update(data)`。

## 常量与类型

| 名称 | 类型 | 说明 |
|---|---|---|
| `hashlib.algorithms_available` | `set[str]` | 当前运行时可用的算法名称集合，如 `{"md5", "sha1", "sha256", ...}` |

**Hash 对象**（由各构造函数返回）

| 属性 | 类型 | 说明 |
|---|---|---|
| `h.name` | `str` | 算法名称，如 `"sha256"` |
| `h.digest_size` | `int` | 摘要字节数 |
| `h.block_size` | `int` | 内部分组字节数 |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `new` | `new(name, data=nil, usedforsecurity=true) → Hash` | 按名称创建哈希对象 |
| `md5` | `md5(data=nil) → Hash` | MD5（128 bit） |
| `sha1` | `sha1(data=nil) → Hash` | SHA-1（160 bit） |
| `sha224` | `sha224(data=nil) → Hash` | SHA-224 |
| `sha256` | `sha256(data=nil) → Hash` | SHA-256 |
| `sha384` | `sha384(data=nil) → Hash` | SHA-384 |
| `sha512` | `sha512(data=nil) → Hash` | SHA-512 |
| `sha3_224` | `sha3_224(data=nil) → Hash` | SHA3-224 |
| `sha3_256` | `sha3_256(data=nil) → Hash` | SHA3-256 |
| `sha3_384` | `sha3_384(data=nil) → Hash` | SHA3-384 |
| `sha3_512` | `sha3_512(data=nil) → Hash` | SHA3-512 |
| `blake2b` | `blake2b(data=nil, digest_size=64, key=nil, salt=nil, person=nil) → Hash` | BLAKE2b（最大 512 bit） |
| `blake2s` | `blake2s(data=nil, digest_size=32, key=nil, salt=nil, person=nil) → Hash` | BLAKE2s（最大 256 bit） |
| `file_digest` | `file_digest(file, name) → Hash` | 分块哈希整个文件 |

**Hash 对象方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `update` | `h.update(data)` | 追加数据到哈希状态 |
| `digest` | `h.digest() → bytes` | 返回当前摘要的二进制字节 |
| `hexdigest` | `h.hexdigest() → str` | 返回当前摘要的小写十六进制字符串 |
| `copy` | `h.copy() → Hash` | 复制当前哈希状态（用于分支计算） |

## 详细语义

### hashlib.new

```
hashlib.new(name, data=nil, usedforsecurity=true) → Hash
```

按名称字符串创建哈希对象。`name` 必须是 `algorithms_available` 中的一个值，
大小写不敏感（`"SHA256"` 与 `"sha256"` 等价）。

- `usedforsecurity=false`：提示运行时该哈希仅用于非安全目的（如校验和），
  允许在受限环境（如 FIPS 模式）中使用 MD5/SHA-1 而不触发安全警告。

`name` 未知时抛 `ValueError`。

---

### hashlib.blake2b / hashlib.blake2s

```
hashlib.blake2b(data=nil, digest_size=64, key=nil, salt=nil, person=nil) → Hash
hashlib.blake2s(data=nil, digest_size=32, key=nil, salt=nil, person=nil) → Hash
```

BLAKE2 支持可变摘要长度与内置键控（keyed hash）模式，可替代 HMAC 使用。

- `digest_size`：blake2b 范围 1–64，blake2s 范围 1–32。
- `key`：bytes，blake2b 最长 64 字节，blake2s 最长 32 字节。非空时启用键控哈希。
- `salt`：bytes，blake2b 最长 16 字节，blake2s 最长 8 字节。用于随机化。
- `person`：bytes，个性化字符串，长度限制同 `salt`。

---

### h.update

```
h.update(data)
```

向哈希对象追加数据。`data` 可以是 `bytes` 或 `str`；传入 `str` 时自动以 UTF-8 编码。
多次调用 `update` 等效于一次性传入所有数据的拼接：

```ms
// h.update(a); h.update(b)  等价于  h.update(a + b)
```

在调用 `digest()` 后再次调用 `update()` 仍合法，会继续累积数据。

---

### h.copy

```
h.copy() → Hash
```

克隆当前哈希状态。用于基于相同前缀数据计算多个不同后缀的摘要，
避免重复处理前缀部分。

---

### hashlib.file_digest

```
hashlib.file_digest(file, name) → Hash
```

对已打开的文件对象 `file` 分块读取并计算哈希，返回最终 Hash 对象。
`name` 为算法名称字符串。文件必须以二进制模式打开（`"rb"`）。

## 示例

```ms
import hashlib

// 1. 计算字符串的 SHA-256 摘要
h := hashlib.sha256("hello, mslang!")
fmt.println(h.hexdigest())
// "3b4f8a9e..." (64 位十六进制字符串)

// 2. 增量更新（适合大数据分块处理）
h2 := hashlib.sha256()
h2.update(bytes("part one "))
h2.update(bytes("part two"))
fmt.println(h2.hexdigest())

// 3. 计算文件的 SHA-256 校验和
with open("archive.tar.gz", "rb") as f {
    digest := hashlib.file_digest(f, "sha256")
    fmt.println("SHA-256:", digest.hexdigest())
}

// 4. BLAKE2b 键控哈希（替代 HMAC）
key := bytes("supersecretkey!!")
h3 := hashlib.blake2b(bytes("message payload"), key=key)
fmt.println(h3.hexdigest())

// 5. 分支计算：公共前缀只处理一次
base := hashlib.sha256(bytes("common prefix "))
h_a := base.copy()
h_b := base.copy()
h_a.update(bytes("branch A"))
h_b.update(bytes("branch B"))
fmt.println(h_a.hexdigest())
fmt.println(h_b.hexdigest())

// 6. 按名称创建，用于动态算法选择
func hash_data(algo, data) {
    return hashlib.new(algo, data).hexdigest()
}
fmt.println(hash_data("sha512", bytes("data")))
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `new()` 传入未知算法名称；`blake2b`/`blake2s` 的 `digest_size`、`key`、`salt`、`person` 超出允许范围 |
| `TypeError` | 传入非 `bytes`/`str` 类型的 `data`；`key`/`salt`/`person` 非 `bytes` 类型 |
| `OSError` | `file_digest` 读取文件时发生 I/O 错误 |
