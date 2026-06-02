# secrets — 密码学安全随机数

```ms
import secrets
```

## 概述

生成适合密码、令牌、会话 ID、API 密钥等**安全敏感场景**的随机数与随机字符串。
底层使用操作系统提供的密码学安全伪随机数生成器（CSPRNG），
在 Linux/macOS 上对应 `/dev/urandom`，在 Windows 上对应 `CryptGenRandom`。

**与 `random` 模块的区别：**

| 特性 | `random` | `secrets` |
|---|---|---|
| 算法 | Mersenne Twister（PRNG） | OS CSPRNG |
| 可预测性 | 知道状态即可预测所有输出 | 不可预测 |
| 适用场景 | 模拟、测试、随机排序 | 密码、令牌、密钥 |
| 需要种子 | 是（或使用默认时间种子） | 否，OS 管理熵 |

不要用 `random` 模块生成任何与安全相关的值。

## 常量与类型

| 名称 | 类型 | 值 | 说明 |
|---|---|---|---|
| `secrets.DEFAULT_ENTROPY` | `int` | `32` | token 函数在 `nbytes=nil` 时使用的默认字节数（256 bit） |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `token_bytes` | `token_bytes(nbytes=nil) → bytes` | 返回随机字节串 |
| `token_hex` | `token_hex(nbytes=nil) → str` | 返回十六进制随机字符串 |
| `token_urlsafe` | `token_urlsafe(nbytes=nil) → str` | 返回 URL 安全 Base64 随机字符串 |
| `choice` | `choice(seq) → item` | 密码学安全随机选取序列元素 |
| `randbelow` | `randbelow(n) → int` | 返回 [0, n) 范围内的随机整数 |
| `randbits` | `randbits(k) → int` | 返回 k 个随机比特组成的非负整数 |

## 详细语义

### secrets.token_bytes

```
secrets.token_bytes(nbytes=nil) → bytes
```

返回 `nbytes` 个随机字节。`nbytes=nil` 时使用 `DEFAULT_ENTROPY`（32 字节）。

适合生成二进制密钥、salt、nonce 等原始随机材料。

---

### secrets.token_hex

```
secrets.token_hex(nbytes=nil) → str
```

生成 `nbytes` 个随机字节，以小写十六进制字符串返回。
输出字符串长度为 `2 * nbytes`。

`nbytes=nil` 时默认 32 字节，输出 64 字符的十六进制字符串。

---

### secrets.token_urlsafe

```
secrets.token_urlsafe(nbytes=nil) → str
```

生成 `nbytes` 个随机字节，以 URL 安全 Base64（RFC 4648）编码返回。
输出只含 `A-Z a-z 0-9 - _`，可直接嵌入 URL 或文件名，无需额外转义。

输出字符串长度约为 `ceil(nbytes * 4 / 3)`，通常带 `=` 填充。

`nbytes=nil` 时默认 32 字节。

---

### secrets.choice

```
secrets.choice(seq) → item
```

从非空序列 `seq` 中以密码学安全方式随机选取并返回一个元素。
`seq` 可以是 `list`、`tuple` 或 `str`。

与 `random.choice` 行为相同，但使用 CSPRNG，适合从字符集中选取密码字符。
`seq` 为空时抛 `ValueError`。

---

### secrets.randbelow

```
secrets.randbelow(n) → int
```

返回 `[0, n)` 范围内均匀分布的随机非负整数。`n` 必须为正整数。
内部使用拒绝采样（rejection sampling）保证均匀性，不存在模偏差（modulo bias）。

`n <= 0` 时抛 `ValueError`。

---

### secrets.randbits

```
secrets.randbits(k) → int
```

返回一个非负整数，其二进制表示恰好含 `k` 个随机比特（即值在 `[0, 2^k)` 范围内）。
`k` 必须为正整数。

**令牌长度建议：**
- **临时令牌**（密码重置、邮件验证）：≥ 16 字节（128 bit）。
- **会话 ID、API 密钥**：≥ 32 字节（256 bit），即 `DEFAULT_ENTROPY`。
- 字节数越多，暴力猜测的代价越高；32 字节在可预见未来内实际不可穷举。

## 示例

```ms
import secrets

// 1. 生成密码重置令牌（URL 安全，可直接放入链接）
reset_token := secrets.token_urlsafe(32)
fmt.println("重置链接: https://example.com/reset?token=" + reset_token)
// 例：https://example.com/reset?token=3d7Kp9...（约 43 字符）

// 2. 生成 API 密钥（十六进制）
api_key := secrets.token_hex(32)
fmt.println("API Key:", api_key)  // 64 字符十六进制字符串

// 3. 生成随机密码（从字符集中选取）
charset := "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!@#$%"
password := ""
for i := 0; i < 16; i++ {
    password = password + secrets.choice(charset)
}
fmt.println("密码:", password)

// 4. 生成 6 位数字 OTP（一次性密码）
otp := secrets.randbelow(1000000)
fmt.printf("OTP: %06d\n", otp)

// 5. 生成加密密钥（原始字节）
aes_key := secrets.token_bytes(32)  // 256-bit AES 密钥
fmt.println("密钥长度:", len(aes_key), "字节")

// 6. 不要这样做（使用 random 模块生成密钥）
// import random
// bad_token := random.token_hex(32)  // 错误！random 是 PRNG，不适合安全用途
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `randbelow(n)` 中 `n <= 0`；`choice(seq)` 中 `seq` 为空序列；`randbits(k)` 中 `k <= 0` |
| `TypeError` | `nbytes` 非整数；`choice` 传入非序列类型 |
| `OSError` | 操作系统 CSPRNG 不可用（极少见，通常发生在内核熵池未初始化时） |
