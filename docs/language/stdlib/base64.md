# base64 — Base16/32/64 编解码

```ms
import base64
```

## 概述

提供二进制数据与文本编码之间的转换工具，适用于 HTTP 传输、JSON 嵌入或文件存储等
需要将任意字节序列表示为可打印字符的场景。支持标准 Base64、URL 安全 Base64、
Base32 和 Base16（十六进制）四种编码方案，以及多行 MIME 编码。

`binascii` 模块的常用功能（`hexlify`/`unhexlify`）已折入本模块，无需单独导入。

所有 `encode` 函数接受 `bytes` 或 `bytearray`，返回 `bytes`；
所有 `decode` 函数接受 `bytes` 或 `str`，返回 `bytes`。

## 常量与类型

本模块不导出常量或类型，所有成员均为函数。

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `b64encode` | `b64encode(data) → bytes` | 标准 Base64 编码 |
| `b64decode` | `b64decode(data, validate=false) → bytes` | 标准 Base64 解码 |
| `urlsafe_b64encode` | `urlsafe_b64encode(data) → bytes` | URL 安全 Base64 编码 |
| `urlsafe_b64decode` | `urlsafe_b64decode(data) → bytes` | URL 安全 Base64 解码 |
| `b32encode` | `b32encode(data) → bytes` | Base32 编码 |
| `b32decode` | `b32decode(data, casefold=false) → bytes` | Base32 解码 |
| `b16encode` | `b16encode(data) → bytes` | Base16（十六进制大写）编码 |
| `b16decode` | `b16decode(data, casefold=false) → bytes` | Base16 解码 |
| `hexlify` | `hexlify(data) → bytes` | `b16encode` 别名（binascii 兼容） |
| `unhexlify` | `unhexlify(data) → bytes` | 十六进制字节串转 bytes（binascii 兼容） |
| `encodebytes` | `encodebytes(data) → bytes` | MIME 多行 Base64 编码 |
| `decodebytes` | `decodebytes(data) → bytes` | MIME 多行 Base64 解码 |

## 详细语义

### Base64（标准）

#### b64encode

```
base64.b64encode(data) → bytes
```

使用标准 Base64 字母表（`A-Z a-z 0-9 + /`）对 `data` 编码，输出以 `=` 填充至 4 字节对齐。

#### b64decode

```
base64.b64decode(data, validate=false) → bytes
```

解码标准 Base64 字节串或字符串。`data` 中的空白字符（空格、换行等）在解码前被忽略。

- `validate=false`（默认）：忽略非 Base64 字符（宽松模式）。
- `validate=true`：严格校验，遇到非法字符或错误填充时抛 `ValueError`。

---

### Base64（URL 安全）

#### urlsafe_b64encode

```
base64.urlsafe_b64encode(data) → bytes
```

与 `b64encode` 相同，但使用 `-` 代替 `+`，`_` 代替 `/`，适用于 URL 和文件名场景。
输出仍包含 `=` 填充。

#### urlsafe_b64decode

```
base64.urlsafe_b64decode(data) → bytes
```

解码 URL 安全 Base64。自动接受 `-`/`_` 或 `+`/`/` 两种字母表。
`data` 的长度不是 4 的倍数时自动补全 `=` 填充后再解码。

---

### Base32

#### b32encode

```
base64.b32encode(data) → bytes
```

使用大写字母 `A-Z` 和数字 `2-7` 对数据编码，输出以 `=` 填充至 8 字节对齐。

#### b32decode

```
base64.b32decode(data, casefold=false) → bytes
```

解码 Base32 字节串。`casefold=true` 时允许小写字母输入。
填充不正确或遇到非法字符时抛 `ValueError`。

---

### Base16 / 十六进制

#### b16encode

```
base64.b16encode(data) → bytes
```

将 `data` 编码为大写十六进制字符串，每字节输出两个十六进制字符。
输出长度始终为输入长度的两倍，无填充。

#### b16decode

```
base64.b16decode(data, casefold=false) → bytes
```

将十六进制字符串解码为原始字节。`casefold=true` 时允许小写十六进制字符。
输入长度为奇数或含非法字符时抛 `ValueError`。

#### hexlify

```
base64.hexlify(data) → bytes
```

`b16encode` 的别名，提供 `binascii.hexlify` 兼容接口。

#### unhexlify

```
base64.unhexlify(data) → bytes
```

`b16decode` 的别名（不支持 `casefold` 参数，仅接受大写十六进制）。
提供 `binascii.unhexlify` 兼容接口。

---

### 多行编码（MIME）

#### encodebytes

```
base64.encodebytes(data) → bytes
```

将 `data` 编码为标准 Base64，并每 76 个字符插入一个换行符 `\n`，最后以换行符结尾。
适用于 MIME 邮件或需要固定行宽的场景。

#### decodebytes

```
base64.decodebytes(data) → bytes
```

解码由 `encodebytes` 生成的多行 Base64 数据，解码前忽略所有空白字符（包括换行）。

## 示例

```ms
import base64

// 1. 标准 Base64 编解码
payload := bytes("hello, mslang!")
encoded := base64.b64encode(payload)
fmt.println(string(encoded))  // "aGVsbG8sIG1zbGFuZyE="

decoded := base64.b64decode(encoded)
fmt.println(string(decoded))  // "hello, mslang!"

// 2. URL 安全 Base64（生成 token）
import crypto   // 假设 crypto.random_bytes 可用
raw_token := crypto.random_bytes(16)
token := base64.urlsafe_b64encode(raw_token)
fmt.println(string(token))  // e.g. "dGhpcyBpcyBhIHRlc3Q="（无 + 或 /）

// 3. 十六进制指纹
fingerprint := base64.hexlify(bytes("\xde\xad\xbe\xef"))
fmt.println(string(fingerprint))  // "DEADBEEF"

raw := base64.unhexlify(bytes("DEADBEEF"))
fmt.println(raw)  // b"\xde\xad\xbe\xef"

// 4. 严格解码（validate=true）
// base64.b64decode("not!!valid", validate=true)  // 抛 ValueError

// 5. MIME 多行编码
data := bytes("A" * 100)
mime_encoded := base64.encodebytes(data)
fmt.println(string(mime_encoded))
// "QUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFB\n
//  QUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQQ==\n"
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `b64decode(validate=true)` 遇到非法字符或错误填充；`b32decode`/`b16decode` 输入非法；`unhexlify` 输入长度为奇数或含非法字符 |
| `TypeError` | 传入非 `bytes`/`bytearray`/`str` 类型的参数 |
