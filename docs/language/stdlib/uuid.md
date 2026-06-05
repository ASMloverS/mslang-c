# uuid — UUID 生成与解析

```ms
import uuid
```

## 概述

生成和解析 UUID（通用唯一标识符），参考 Python `uuid` 模块与 RFC 4122。支持
版本 1（时间 + MAC）、版本 3（MD5 哈希）、版本 4（随机）、版本 5（SHA-1 哈希）。
`uuid.UUID` 对象不可变，可用作 map 键。

## 常量与类型

### 命名空间常量

| 常量 | 值（标准 UUID） | 用途 |
|---|---|---|
| `uuid.NAMESPACE_DNS` | `6ba7b810-9dad-11d1-80b4-00c04fd430c8` | DNS 主机名 |
| `uuid.NAMESPACE_URL` | `6ba7b811-9dad-11d1-80b4-00c04fd430c8` | URL |
| `uuid.NAMESPACE_OID` | `6ba7b812-9dad-11d1-80b4-00c04fd430c8` | ISO OID |
| `uuid.NAMESPACE_X500` | `6ba7b814-9dad-11d1-80b4-00c04fd430c8` | X.500 DN |

### uuid.UUID

不可变的 UUID 值类型。

**构造器：**

```
uuid.UUID(hex=nil, bytes=nil, fields=nil, version=nil)
```

必须且只能传入 `hex`、`bytes`、`fields` 三者之一：

- `hex`：32 位十六进制字符串，连字符可选（如 `"550e8400-e29b-41d4-a716-446655440000"`
  或 `"550e8400e29b41d4a716446655440000"`）。
- `bytes`：16 字节的 `bytes` 对象（大端序）。
- `fields`：6 元素 tuple `(timeLow, timeMid, timeHiVersion,
  clockSeqHiVariant, clockSeqLow, node)`，各为 int。

`version`（可选）：若提供，强制设置 UUID 版本位和变体位（1–5）。

**只读属性：**

| 属性 | 类型 | 说明 |
|---|---|---|
| `.hex` | `str` | 32 个十六进制字符，无连字符 |
| `.bytes` | `bytes` | 16 字节大端序 |
| `.int` | `int` | 128 位整数 |
| `.urn` | `str` | `"urn:uuid:xxxxxxxx-..."` |
| `.version` | `int \| nil` | UUID 版本（1–5），无版本时为 nil |
| `.variant` | `str` | `"RFC 4122"`、`"reserved"`、`"microsoft"` 等 |
| `.fields` | `tuple` | 6 元素 tuple（同构造器 fields 参数） |
| `.timeLow` | `int` | fields[0] |
| `.timeMid` | `int` | fields[1] |
| `.timeHiVersion` | `int` | fields[2] |
| `.clockSeqHiVariant` | `int` | fields[3] |
| `.clockSeqLow` | `int` | fields[4] |
| `.node` | `int` | fields[5]（48 位 MAC 地址） |

`str(u)` 返回标准带连字符格式：`"xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"`。

支持 `==`、`!=`、`<`（按 `.int` 比较）及 `hash`（可用作 map 键）。

**哨兵值：**

```ms
uuid.UUID.INVALID  // 等同于 uuid.UUID(hex="00000000000000000000000000000000")
```

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `uuid1` | `uuid.uuid1(node=nil, clockSeq=nil) → UUID` | 版本 1：时间 + MAC |
| `uuid3` | `uuid.uuid3(namespace, name) → UUID` | 版本 3：MD5 哈希，确定性 |
| `uuid4` | `uuid.uuid4() → UUID` | 版本 4：随机 |
| `uuid5` | `uuid.uuid5(namespace, name) → UUID` | 版本 5：SHA-1 哈希，确定性 |

## 详细语义

### uuid.uuid1

```
uuid.uuid1(node=nil, clockSeq=nil) → UUID
```

基于当前时间戳和 MAC 地址生成版本 1 UUID。

- `node`：48 位整数，指定 MAC 地址；省略时使用系统 MAC（不可用时随机生成）。
- `clockSeq`：14 位时钟序列；省略时随机生成。

**注意**：UUID 1 含有 MAC 地址，存在隐私泄露风险；多数场景推荐使用 `uuid4`。

### uuid.uuid3

```
uuid.uuid3(namespace, name) → UUID
```

对 `namespace.bytes + name.encode("utf-8")` 取 MD5，生成版本 3 UUID。
相同输入永远产生相同输出（确定性），适用于从已知数据派生固定 ID 的场景。
`namespace` 为 `UUID` 实例（通常使用内置命名空间常量）。

### uuid.uuid4

```
uuid.uuid4() → UUID
```

使用密码学安全随机数（内部调用 `secrets` 模块）生成版本 4 UUID。
每次调用结果不同，碰撞概率极低。最常用的 UUID 生成方式。

### uuid.uuid5

```
uuid.uuid5(namespace, name) → UUID
```

与 `uuid3` 相同，但使用 SHA-1 而非 MD5。RFC 4122 推荐优先使用 uuid5，
因其哈希强度更高。

## 示例

```ms
import uuid
import fmt

// 1. 生成随机 UUID（最常用）
u := uuid.uuid4()
fmt.println(str(u))          // "110e8400-e29b-41d4-a716-446655440000"（示例）
fmt.println(u.hex)           // "110e8400e29b41d4a716446655440000"
fmt.println(u.version)       // 4

// 2. 解析 UUID 字符串
parsed := uuid.UUID(hex="550e8400-e29b-41d4-a716-446655440000")
fmt.println(parsed.int)      // 大整数

// 3. 从 bytes 构造
raw := bytes("\x55\x0e\x84\x00\xe2\x9b\x41\xd4\xa7\x16\x44\x66\x55\x44\x00\x00")
u2 := uuid.UUID(bytes=raw)
fmt.println(str(u2))

// 4. uuid5 生成确定性 ID（如用 URL 命名空间为资源生成稳定 ID）
resourceId := uuid.uuid5(uuid.NAMESPACE_URL, "https://example.com/user/42")
fmt.println(str(resourceId))  // 固定值，相同输入始终相同

// 5. 用作 map 键
cache := {}
key := uuid.uuid4()
cache[key] = "session_data"
fmt.println(cache[key])      // "session_data"

// 6. 比较与判等
a := uuid.UUID(hex="00000000000000000000000000000001")
b := uuid.UUID(hex="00000000000000000000000000000002")
fmt.println(a < b)           // true
fmt.println(a == uuid.UUID.INVALID)  // false
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `ValueError` | `hex` 字符串长度不为 32（去除连字符后）或含非十六进制字符 |
| `ValueError` | `bytes` 长度不为 16 |
| `ValueError` | `fields` 元组元素数量不为 6，或各字段超出合法范围 |
| `ValueError` | `version` 不在 1–5 范围内 |
| `TypeError` | 同时传入多个构造参数（hex/bytes/fields 互斥） |
| `TypeError` | 未传入任何构造参数 |
