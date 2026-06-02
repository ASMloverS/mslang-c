# hmac — 基于哈希的消息认证码

```ms
import hmac
```

## 概述

实现 HMAC（Hash-based Message Authentication Code，RFC 2104），
用于同时验证消息的**完整性**（未被篡改）与**来源真实性**（持有密钥方发出）。
参考 Python hmac 模块设计。

与单纯的哈希相比，HMAC 将密钥混入运算，可抵抗**长度扩展攻击**（length extension attack）：
即使攻击者知道 `HMAC(key, msg)` 的值，也无法在不知道密钥的情况下构造
`HMAC(key, msg + extra)` 的合法值。

验证 HMAC 时**必须使用 `hmac.compare_digest`** 而非 `==` 运算符，
以防止基于响应时间的**时序攻击**（timing attack）。

## 常量与类型

本模块不导出常量。

**HMAC 对象**（由 `hmac.new` 返回）

| 属性 | 类型 | 说明 |
|---|---|---|
| `h.digest_size` | `int` | 底层哈希算法的摘要字节数 |
| `h.name` | `str` | 形如 `"hmac-sha256"` 的标识符 |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `new` | `new(key, msg=nil, digestmod="sha256") → HMAC` | 创建 HMAC 对象 |
| `digest` | `digest(key, msg, digest) → bytes` | 一次性计算 HMAC 摘要 |
| `compare_digest` | `compare_digest(a, b) → bool` | 常量时间比较，防时序攻击 |

**HMAC 对象方法**

| 方法 | 签名 | 说明 |
|---|---|---|
| `update` | `h.update(msg)` | 追加数据 |
| `digest` | `h.digest() → bytes` | 返回二进制摘要 |
| `hexdigest` | `h.hexdigest() → str` | 返回十六进制摘要 |
| `copy` | `h.copy() → HMAC` | 复制当前 HMAC 状态 |

## 详细语义

### hmac.new

```
hmac.new(key, msg=nil, digestmod="sha256") → HMAC
```

创建并返回一个 HMAC 对象。

- `key`：`bytes`，认证密钥。长度无硬性限制，但推荐与底层哈希摘要长度相同
  （SHA-256 对应 32 字节）。密钥过短会降低安全性。
- `msg`：可选初始消息，`bytes` 类型。等效于构造后立即调用 `update(msg)`。
- `digestmod`：底层哈希算法名称字符串（`"sha256"`、`"sha512"` 等），
  或直接传入 `hashlib` 算法名称。默认 `"sha256"`。

---

### hmac.digest

```
hmac.digest(key, msg, digest) → bytes
```

一次性计算 `HMAC(key, msg)` 并直接返回原始字节摘要，
等效于 `hmac.new(key, msg, digest).digest()`，但内部实现更高效（避免对象分配）。

- `key`：`bytes`，密钥。
- `msg`：`bytes`，待认证消息。
- `digest`：算法名称字符串，如 `"sha256"`。

---

### hmac.compare_digest

```
hmac.compare_digest(a, b) → bool
```

以**常量时间**比较两个字节串或字符串是否相等。

**为什么不能用 `a == b`？**
普通字符串比较在遇到第一个不同字节时立即返回，比较耗时与两个值的公共前缀长度正相关。
攻击者可以通过多次发送不同猜测值并精确测量服务器响应时间，逐字节推断出正确的 HMAC 值。
`compare_digest` 无论 `a` 与 `b` 在哪个位置开始不同，始终花费相同时间，
从根本上消除这一信息泄露渠道。

- `a`、`b`：均为 `bytes` 或均为 `str`（类型必须一致）。
- 返回 `true` 当且仅当两者内容完全相同。
- 即使 `a` 与 `b` 长度不同，函数仍安全返回 `false`，不泄露长度差异信息。

---

### h.update

```
h.update(msg)
```

向 HMAC 对象追加数据。`msg` 必须为 `bytes`。
多次调用等效于一次性传入所有数据的拼接。

---

### h.copy

```
h.copy() → HMAC
```

克隆当前 HMAC 状态，用于基于相同前缀计算多个不同后缀的认证码。

## 示例

```ms
import hmac
import base64

secret_key := bytes("my-secret-key-32bytes-padded!!!")

// 1. 为消息生成 HMAC
msg := bytes("user_id=42&action=transfer&amount=100")
mac := hmac.new(secret_key, msg, "sha256")
tag := mac.hexdigest()
fmt.println("HMAC:", tag)

// 2. 接收方验证 HMAC（使用 compare_digest 防时序攻击）
func verify_hmac(key, msg, received_tag) {
    expected := hmac.new(key, msg, "sha256").digest()
    // received_tag 是 bytes 类型
    return hmac.compare_digest(expected, received_tag)
}

received := hmac.new(secret_key, msg, "sha256").digest()
if verify_hmac(secret_key, msg, received) {
    fmt.println("消息认证通过")
} else {
    fmt.println("消息已被篡改！")
}

// 3. 一次性接口（更简洁高效）
tag_bytes := hmac.digest(secret_key, msg, "sha512")
fmt.println("HMAC-SHA512:", base64.b64encode(tag_bytes))

// 4. 错误示范（不应这样验证 HMAC）
// if mac.hexdigest() == received_tag { ... }  // 存在时序攻击风险！
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `digestmod` 指定了未知算法名称；`key` 为空 bytes |
| `TypeError` | `key` 或 `msg` 非 `bytes` 类型；`compare_digest` 的两个参数类型不一致（一个 `str` 一个 `bytes`） |
