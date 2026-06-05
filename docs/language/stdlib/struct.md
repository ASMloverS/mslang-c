# struct — 二进制数据打包与解包

```ms
import struct
```

## 概述

在 mslang 值与 C 结构体风格的二进制字节串之间相互转换。格式串语法与 Python `struct`
模块完全兼容，适用于解析网络协议报文、读写二进制文件格式和处理底层二进制 I/O。

所有打包函数返回 `bytes`；解包函数接受 `bytes`、`bytearray` 或支持缓冲区协议的对象。

## 常量与类型

| 名称 | 说明 |
|---|---|
| `struct.Error` | 打包/解包错误，`ValueError` 的子类 |
| `struct.Struct` | 预编译格式对象类，用于重复使用同一格式 |

## 函数签名速查

| 函数 | 签名 | 说明 |
|---|---|---|
| `pack` | `pack(fmt, *values) → bytes` | 将值打包为字节串 |
| `unpack` | `unpack(fmt, buffer) → tuple` | 将字节串解包为值元组 |
| `packInto` | `packInto(fmt, buffer, offset, *values)` | 原地打包到可变缓冲区 |
| `unpackFrom` | `unpackFrom(fmt, buffer, offset=0) → tuple` | 从缓冲区偏移处解包 |
| `calcsize` | `calcsize(fmt) → int` | 计算格式串对应的字节数 |
| `iterUnpack` | `iterUnpack(fmt, buffer) → iterator` | 惰性解包多条记录 |

## 详细语义

### 格式串语法

格式串由可选的**字节序前缀**和一个或多个**类型码**（可加数量前缀）组成。

**字节序前缀**（放在格式串最前面，最多一个）

| 字符 | 字节序 | 对齐 |
|---|---|---|
| `@` | 原生字节序 | 原生对齐 |
| `=` | 原生字节序 | 无对齐填充 |
| `<` | 小端（Little-Endian） | 无对齐填充 |
| `>` | 大端（Big-Endian） | 无对齐填充 |
| `!` | 网络字节序（= 大端） | 无对齐填充 |

不指定前缀时默认使用 `@`（原生字节序 + 原生对齐）。跨平台代码应始终显式指定字节序。

**类型码表**

| 码 | C 类型 | mslang 类型 | 标准大小 |
|---|---|---|---|
| `x` | pad byte（填充） | （无对应值） | 1 |
| `c` | `char` | `bytes`（长度 1） | 1 |
| `b` | `signed char` | `int` | 1 |
| `B` | `unsigned char` | `int` | 1 |
| `?` | `_Bool` | `bool` | 1 |
| `h` | `short` | `int` | 2 |
| `H` | `unsigned short` | `int` | 2 |
| `i` | `int` | `int` | 4 |
| `I` | `unsigned int` | `int` | 4 |
| `l` | `long` | `int` | 4 |
| `L` | `unsigned long` | `int` | 4 |
| `q` | `long long` | `int` | 8 |
| `Q` | `unsigned long long` | `int` | 8 |
| `f` | `float` | `float` | 4 |
| `d` | `double` | `float` | 8 |
| `s` | `char[]` | `bytes` | 由计数决定 |
| `p` | Pascal string | `bytes` | 由计数决定 |
| `n` | `ssize_t` | `int` | 原生大小（仅 `@` 字节序，无标准大小） |
| `N` | `size_t` | `int` | 原生大小（仅 `@` 字节序，无标准大小） |

**数量前缀**：类型码前可以加整数表示重复次数，例如 `4B` 等价于 `BBBB`，
`10s` 表示长度为 10 的 `bytes`（不是 10 个单字节）。

`x`（填充字节）在打包时写入零字节，在解包时跳过，不产生对应的值。

---

### 函数

#### pack

```
struct.pack(fmt, *values) → bytes
```

按 `fmt` 将 `values` 打包为字节串。`values` 的数量和类型必须与格式串精确匹配，
否则抛 `struct.Error`。

#### unpack

```
struct.unpack(fmt, buffer) → tuple
```

按 `fmt` 将 `buffer` 解包为值元组。`buffer` 的字节数必须恰好等于 `struct.calcsize(fmt)`，
否则抛 `struct.Error`。

#### packInto

```
struct.packInto(fmt, buffer, offset, *values)
```

将值打包后写入 `buffer`（必须为 `bytearray` 或其他可写缓冲区）的 `offset` 偏移处。
`buffer` 从 `offset` 起必须有足够空间，否则抛 `struct.Error`。

#### unpackFrom

```
struct.unpackFrom(fmt, buffer, offset=0) → tuple
```

从 `buffer` 的 `offset` 偏移处解包，只读取 `calcsize(fmt)` 个字节，
`buffer` 余下的字节被忽略。`buffer` 从 `offset` 起必须至少有 `calcsize(fmt)` 字节，
否则抛 `struct.Error`。

#### calcsize

```
struct.calcsize(fmt) → int
```

返回格式串 `fmt` 对应的字节总数（包含对齐填充）。用于预分配缓冲区或验证数据长度。

#### iterUnpack

```
struct.iterUnpack(fmt, buffer) → iterator
```

将 `buffer` 按 `calcsize(fmt)` 字节一组惰性解包，产生元组序列。
`buffer` 长度必须是 `calcsize(fmt)` 的整数倍，否则抛 `struct.Error`。
`fmt` 中不能含有可变长度类型（如 `s`/`p` 不带固定计数前缀时）。

---

### struct.Struct 类

```
struct.Struct(fmt)
```

预编译的格式对象，适合在循环中重复使用同一格式以避免重复解析开销。

**方法与属性**

| 成员 | 说明 |
|---|---|
| `s.pack(*values) → bytes` | 等价于 `struct.pack(s.format, *values)` |
| `s.unpack(buffer) → tuple` | 等价于 `struct.unpack(s.format, buffer)` |
| `s.packInto(buffer, offset, *values)` | 等价于 `struct.packInto(s.format, buffer, offset, *values)` |
| `s.unpackFrom(buffer, offset=0) → tuple` | 等价于 `struct.unpackFrom(s.format, buffer, offset)` |
| `s.iterUnpack(buffer) → iterator` | 等价于 `struct.iterUnpack(s.format, buffer)` |
| `s.size → int` | 只读，等价于 `struct.calcsize(s.format)` |
| `s.format → str` | 只读，构造时传入的格式串 |

---

### struct.Error

`struct.Error` 是 `ValueError` 的子类，在以下情况下抛出：

- 值的数量或类型与格式串不匹配
- 缓冲区大小与格式串要求不符
- 数值超出类型范围（如将 300 打包为 `B`）
- 格式串语法错误

## 示例

```ms
import struct

// 1. 打包/解包网络协议报头
// 格式：魔数(4s) + 版本(H) + 长度(I)，大端字节序
HEADER_FMT := ">4sHI"
header := struct.pack(HEADER_FMT, bytes("MSCP"), 1, 1024)
fmt.println(header)  // b'\x4d\x53\x43\x50\x00\x01\x00\x00\x04\x00'

magic, version, length := struct.unpack(HEADER_FMT, header)
fmt.println(string(magic), version, length)  // "MSCP" 1 1024

// 2. 解析二进制文件中的多条定长记录
// 每条记录：name(16s) + score(f) + rank(H)
RECORD_FMT := "<16sfH"
recordSize := struct.calcsize(RECORD_FMT)
fmt.println(recordSize)  // 22

// 模拟二进制数据
buf := struct.pack(RECORD_FMT, bytes("Alice\x00" * 3)[0:16], 98.5, 1) +
       struct.pack(RECORD_FMT, bytes("Bob\x00" * 5)[0:16], 87.0, 2)

for name, score, rank in struct.iterUnpack(RECORD_FMT, buf) {
    fmt.println(string(name).strip("\x00"), score, rank)
}
// Alice 98.5 1
// Bob   87.0 2

// 3. 使用 Struct 对象重复解包（性能更好）
s := struct.Struct(">HHI")
buf2 := bytearray(s.size * 3)
s.packInto(buf2, 0,       1, 2, 100)
s.packInto(buf2, s.size,  3, 4, 200)
s.packInto(buf2, s.size*2, 5, 6, 300)

for a, b, c in s.iterUnpack(buf2) {
    fmt.println(a, b, c)
}
// 1 2 100
// 3 4 200
// 5 6 300

// 4. calcsize 用于预分配
fmt.println(struct.calcsize("!BBHIH6s"))  // 1+1+2+4+2+6 = 16（无对齐）
```

## 本模块异常

| 异常 | 触发条件 |
|---|---|
| `struct.Error` (`ValueError` 子类) | 值数量/类型不匹配；缓冲区大小错误；数值超出范围；格式串语法错误 |
| `TypeError` | `packInto` 的 `buffer` 不可写；`buffer` 类型不受支持 |
